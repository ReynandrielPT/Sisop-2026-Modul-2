#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ─── Queue names ─────────────────────────────────────────────────────────── */
#define MQ_BS_JOIN    "/mq_bs_join"
#define MQ_P1_TO_SRV  "/mq_bs_p1_srv"
#define MQ_P2_TO_SRV  "/mq_bs_p2_srv"
#define MQ_SRV_TO_P1  "/mq_bs_srv_p1"
#define MQ_SRV_TO_P2  "/mq_bs_srv_p2"
#define MSG_SIZE      512

/* ─── Game constants ──────────────────────────────────────────────────────── */
#define ROWS          5
#define COLS          5
#define MAX_SHIPS     5          /* At most 3 ships (S+S+S uses 3 slots) */
#define FLEET_MAX_PTS 7

/* ─── Ship types ──────────────────────────────────────────────────────────── */
#define SHIP_NONE       0
#define SHIP_SUBMARINE  'S'
#define SHIP_DESTROYER  'D'
#define SHIP_CRUISER    'C'

/* ─── Cell markers ────────────────────────────────────────────────────────── */
#define CELL_EMPTY   '.'
#define CELL_HIT     'X'

/* ─── Structures ──────────────────────────────────────────────────────────── */
typedef struct {
    char type;          /* S / D / C */
    int  tiles[3][2];  /* row,col of each tile (max 3 for Cruiser) */
    int  size;          /* number of tiles */
    int  hits;          /* tiles hit so far */
    int  sunk;          /* 1 if sunk */
    int  cooldown;      /* turns remaining before can fire again */
    int  index;         /* 1-based index within same type, for display */
} Ship;

typedef struct {
    /* Own board: stores ship symbols or CELL_EMPTY */
    char board[ROWS][COLS];
    /* Hit mask: 'X' where opponent hit */
    char hit_board[ROWS][COLS];
    Ship ships[MAX_SHIPS];
    int  num_ships;
    int  ships_remaining;
    int  connected;
} Player;

/* ─── Globals ────────────────────────────────────────────────────────────── */
static Player players[2];
static mqd_t mq_in[2], mq_out[2];

/* ─── Utilities ──────────────────────────────────────────────────────────── */
static int col_to_idx(char c) {
    if (c >= 'a' && c <= 'e') return c - 'a';
    if (c >= 'A' && c <= 'E') return c - 'A';
    return -1;
}

static void srv_send(int pid, const char *msg) {
    mq_send(mq_out[pid], msg, strlen(msg) + 1, 0);
}

static void srv_recv(int pid, char *buf) {
    memset(buf, 0, MSG_SIZE);
    mq_receive(mq_in[pid], buf, MSG_SIZE, NULL);
}

/* ─── Board helpers ──────────────────────────────────────────────────────── */
static void init_board(Player *p) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            p->board[r][c]     = CELL_EMPTY;
            p->hit_board[r][c] = CELL_EMPTY;
        }
}

/* ─── Fleet parsing ──────────────────────────────────────────────────────── */
/*
 * PLACE messages from player arrive as tokens:
 *   <row><COL>  e.g. "2C 4C"  means tile (2,C) to (4,C)
 * One PLACE message per ship, prefixed with ship type and ship index:
 *   "PLACE S 1 0A 0B"
 *   "PLACE D 1 2C 3C"
 *   "PLACE C 1 1A 1C"
 */
static int place_ship(Player *p, char type, int idx, int r1, int c1, int r2, int c2) {
    int size = (type == SHIP_CRUISER) ? 3 : 2;

    /* Determine direction */
    int dr = 0, dc = 0;
    if (r1 == r2) dc = (c2 > c1) ? 1 : -1;
    else          dr = (r2 > r1) ? 1 : -1;

    /* Check span */
    int span = (r1 == r2) ? abs(c2 - c1) + 1 : abs(r2 - r1) + 1;
    if (span != size) return -1;          /* wrong tile count */
    if (r1 != r2 && c1 != c2) return -2; /* diagonal */

    /* Validate bounds and occupation */
    int r = r1, c = c1;
    for (int i = 0; i < size; i++) {
        if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return -3; /* out of bounds */
        if (p->board[r][c] != CELL_EMPTY) return -4;              /* occupied */
        r += dr; c += dc;
    }

    /* Place the ship */
    Ship *s = &p->ships[p->num_ships];
    s->type  = type;
    s->size  = size;
    s->hits  = 0;
    s->sunk  = 0;
    s->cooldown = 0;
    s->index = idx;

    r = r1; c = c1;
    for (int i = 0; i < size; i++) {
        s->tiles[i][0] = r;
        s->tiles[i][1] = c;
        p->board[r][c] = (char)type;
        r += dr; c += dc;
    }
    p->num_ships++;
    p->ships_remaining++;
    return 0;
}

/* ─── Fleet selection phase ──────────────────────────────────────────────── */
/*
 * The player sends a FLEET message: "FLEET 1S 2D" or "FLEET 1S 1D 1C" etc.
 * Server validates & responds FLEET_OK or FLEET_ERR.
 * Then the player sends PLACE messages for each ship.
 */
static int pts_for(char t) {
    if (t == SHIP_SUBMARINE || t == SHIP_DESTROYER) return 2;
    if (t == SHIP_CRUISER) return 3;
    return 0;
}

static int cooldown_for(char t) {
    if (t == SHIP_DESTROYER) return 2;
    if (t == SHIP_CRUISER)   return 3;
    return 0;
}

static void setup_player(int pid) {
    Player *p = &players[pid];
    char buf[MSG_SIZE];

    /* Expect FLEET message */
    while (1) {
        srv_recv(pid, buf);
        /* Parse "1S 2D" etc. */
        int ns = 0, nd = 0, nc = 0;
        char *tok = strtok(buf, " ");
        int valid = 1;
        while (tok) {
            int qty = 0;
            char type = 0;
            if (sscanf(tok, "%d%c", &qty, &type) == 2) {
                if (type == 'S' || type == 's') ns += qty;
                else if (type == 'D' || type == 'd') nd += qty;
                else if (type == 'C' || type == 'c') nc += qty;
                else { valid = 0; break; }
            } else { valid = 0; break; }
            tok = strtok(NULL, " ");
        }
        if (!valid || (ns + nd + nc) < 1) {
            srv_send(pid, "FLEET_ERR Invalid ship types.");
            continue;
        }
        int total_pts = ns * 2 + nd * 2 + nc * 3;
        if (total_pts > FLEET_MAX_PTS) {
            srv_send(pid, "FLEET_ERR Fleet exceeds 7 point budget.");
            continue;
        }
        /* Store fleet counts for placement loop */
        char ok_msg[MSG_SIZE];
        snprintf(ok_msg, MSG_SIZE, "FLEET_OK %dS %dD %dC", ns, nd, nc);
        srv_send(pid, ok_msg);

        /* Now handle PLACE messages */
        /* Collect ship types to place in order: S, D, C */
        char order[MAX_SHIPS];
        int  order_idx[MAX_SHIPS];
        int oc = 0;
        for (int i = 1; i <= ns; i++) { order[oc] = SHIP_SUBMARINE; order_idx[oc++] = i; }
        for (int i = 1; i <= nd; i++) { order[oc] = SHIP_DESTROYER;  order_idx[oc++] = i; }
        for (int i = 1; i <= nc; i++) { order[oc] = SHIP_CRUISER;    order_idx[oc++] = i; }

        for (int si = 0; si < oc; si++) {
            while (1) {
                srv_recv(pid, buf);
                /* Expect "<type> <idx> <r1c1> <r2c2>" */
                char pt; int pi;
                char coord1_str[8], coord2_str[8];
                if (sscanf(buf, "%c %d %s %s", &pt, &pi, coord1_str, coord2_str) != 4) {
                    srv_send(pid, "PLACE_ERR Invalid format (Expected <type> <idx> <r1c1> <r2c2>).");
                    continue;
                }

                /* Validate ship type and index match expected */
                if (pt != order[si] || pi != order_idx[si]) {
                    char em[MSG_SIZE];
                    snprintf(em, MSG_SIZE, "PLACE_ERR Expected ship %c%d, got %c%d.",
                             order[si], order_idx[si], pt, pi);
                    srv_send(pid, em);
                    continue;
                }

                /* Parse coords */
                int r1 = coord1_str[0] - '0';
                int c1 = col_to_idx(coord1_str[1]);
                int r2 = coord2_str[0] - '0';
                int c2 = col_to_idx(coord2_str[1]);

                if (r1 < 0 || r1 >= ROWS || r2 < 0 || r2 >= ROWS ||
                    c1 < 0 || c2 < 0) {
                    srv_send(pid, "PLACE_ERR Coordinate out of bounds!");
                    continue;
                }

                int res = place_ship(p, order[si], order_idx[si], r1, c1, r2, c2);
                if (res == -1) {
                    char em[MSG_SIZE];
                    snprintf(em, MSG_SIZE, "PLACE_ERR %c needs exactly %d adjacent tiles!",
                             order[si], (order[si] == SHIP_CRUISER) ? 3 : 2);
                    srv_send(pid, em);
                } else if (res == -2) {
                    srv_send(pid, "PLACE_ERR The tiles are not adjacent!");
                } else if (res == -3) {
                    srv_send(pid, "PLACE_ERR Coordinate out of bounds!");
                } else if (res == -4) {
                    srv_send(pid, "PLACE_ERR Tiles are already occupied!");
                } else {
                    char ok[MSG_SIZE];
                    if (ns + nd + nc > 1)
                        snprintf(ok, MSG_SIZE, "PLACE_OK %c%d placed.", order[si], order_idx[si]);
                    else
                        snprintf(ok, MSG_SIZE, "PLACE_OK %c placed.", order[si]);
                    srv_send(pid, ok);
                    break;
                }
            }
        }
        /* Send board state to player */
        char board_msg[MSG_SIZE * 4];
        int bm = 0;
        bm += snprintf(board_msg + bm, sizeof(board_msg) - bm, "BOARD_INIT ");
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                bm += snprintf(board_msg + bm, sizeof(board_msg) - bm, "%c", p->board[r][c]);
        srv_send(pid, board_msg);
        break;
    }
}

/* ─── Fire processing ────────────────────────────────────────────────────── */
/*
 * FIRE message: "FIRE S 2C" or "FIRE D 0A 3E" or "FIRE C 0B 1D 4A"
 * Server returns FIRE_RES with hit/miss per tile and sunk info.
 */
static void process_fire(int shooter_pid) {
    int target_pid = 1 - shooter_pid;
    Player *shooter = &players[shooter_pid];
    Player *target  = &players[target_pid];
    char buf[MSG_SIZE];

    while (1) {
        srv_recv(shooter_pid, buf);
        /* Expect "<type> <coords...>" or "SKIP" */

        if (strcmp(buf, "SKIP") == 0) {
            printf("[TURN] Player %d: all ships on cooldown. Turn skipped.\n",
                   shooter_pid + 1);
            srv_send(shooter_pid, "FIRE_RES You skipped your turn."); // Added this for client feedback
            return;
        }

        char ship_type;
        int ship_idx = 0;
        char coords[3][8];
        int  num_coords = 0;
        char *tok = strtok(buf, " ");
        if (!tok) {
            srv_send(shooter_pid, "FIRE_ERR Invalid action format.");
            goto next_input;
        }
        ship_type = tok[0];
        if (strlen(tok) > 1 && tok[1] >= '0' && tok[1] <= '9') {
            ship_idx = tok[1] - '0';
        }
        
        tok = strtok(NULL, " ");
        while (tok && num_coords < 3) {
            strncpy(coords[num_coords], tok, 7);
            coords[num_coords][7] = '\0'; // Ensure null termination
            num_coords++;
            tok = strtok(NULL, " ");
        }

        /* Find the ship */
        Ship *chosen = NULL;
        for (int i = 0; i < shooter->num_ships; i++) {
            if (shooter->ships[i].type == ship_type && !shooter->ships[i].sunk) {
                if (ship_idx > 0 && shooter->ships[i].index != ship_idx) continue;
                
                if (shooter->ships[i].cooldown == 0) {
                    chosen = &shooter->ships[i];
                    break;
                }
            }
        }
        if (!chosen) {
            srv_send(shooter_pid, "FIRE_ERR No matched ship available or it is on cooldown.");
            goto next_input;
        }

        /* Validate coordinate count */
        int expected = (ship_type == SHIP_SUBMARINE) ? 1 :
                       (ship_type == SHIP_DESTROYER)  ? 2 : 3;
        if (num_coords != expected) {
            char em[MSG_SIZE];
            snprintf(em, MSG_SIZE, "FIRE_ERR Expected %d coordinate(s) for this ship.", expected);
            srv_send(shooter_pid, em);
            goto next_input;
        }

        /* Validate all coords in bounds */
        int valid = 1;
        for (int i = 0; i < num_coords; i++) {
            if (strlen(coords[i]) != 2) { valid = 0; break; } // Ensure format like "0A"
            int r = coords[i][0] - '0';
            int c = col_to_idx(coords[i][1]);
            if (r < 0 || r >= ROWS || c < 0) { valid = 0; break; }
        }
        if (!valid) {
            srv_send(shooter_pid, "FIRE_ERR Coordinate out of bounds or invalid format!");
            goto next_input;
        }

        {
            /* Process each coordinate */
            char result_msg[MSG_SIZE * 2];
            char server_log[MSG_SIZE * 2];
            char coords_str[MSG_SIZE] = "";
            int hits = 0, misses = 0;
            int rm = 0, sm = 0;

            /* Build coords string for log */
            for (int i = 0; i < num_coords; i++) {
                if (i > 0) strncat(coords_str, ", ", sizeof(coords_str) - strlen(coords_str) - 1);
                strncat(coords_str, coords[i], sizeof(coords_str) - strlen(coords_str) - 1);
            }

            rm += snprintf(result_msg + rm, sizeof(result_msg) - rm, "FIRE_RES ");
            sm += snprintf(server_log + sm, sizeof(server_log) - sm,
                           "[TURN] Player %d fired using %c at %s\n",
                           shooter_pid + 1, ship_type, coords_str);

            char sunk_ship_name[64] = "";
            for (int i = 0; i < num_coords; i++) {
                int r = coords[i][0] - '0';
                int c = col_to_idx(coords[i][1]);
                int is_hit = 0;

                /* Check against target's ships */
                for (int si = 0; si < target->num_ships; si++) {
                    Ship *ts = &target->ships[si];
                    if (ts->sunk) continue;
                    for (int ti = 0; ti < ts->size; ti++) {
                        if (ts->tiles[ti][0] == r && ts->tiles[ti][1] == c) {
                            ts->hits++;
                            target->hit_board[r][c] = CELL_HIT;
                            is_hit = 1;
                            sm += snprintf(server_log + sm, sizeof(server_log) - sm,
                                           "  - %s: HIT\n", coords[i]);
                            rm += snprintf(result_msg + rm, sizeof(result_msg) - rm,
                                           "HIT:%s ", coords[i]);
                            hits++;

                            if (ts->hits == ts->size) {
                                ts->sunk = 1;
                                target->ships_remaining--;
                                if (ts->size > 1 || 1) {
                                    const char *sname =
                                        (ts->type == SHIP_SUBMARINE) ? "Submarine" :
                                        (ts->type == SHIP_DESTROYER)  ? "Destroyer" : "Cruiser";
                                    snprintf(sunk_ship_name, sizeof(sunk_ship_name), "%s", sname);
                                }
                            }
                            goto next_coord;
                        }
                    }
                }
                /* Miss */
                sm += snprintf(server_log + sm, sizeof(server_log) - sm,
                               "  - %s: MISS\n", coords[i]);
                rm += snprintf(result_msg + rm, sizeof(result_msg) - rm,
                               "MISS:%s ", coords[i]);
                misses++;
                next_coord:;
            }

            sm += snprintf(server_log + sm, sizeof(server_log) - sm,
                           "  Result: %d hit(s), %d miss(es)\n", hits, misses);
            rm += snprintf(result_msg + rm, sizeof(result_msg) - rm,
                           "HITS:%d MISSES:%d", hits, misses);

            if (sunk_ship_name[0]) {
                sm += snprintf(server_log + sm, sizeof(server_log) - sm,
                               "  [SUNK] Player %d sank Player %d's %s!\n",
                               shooter_pid + 1, target_pid + 1, sunk_ship_name);
                rm += snprintf(result_msg + rm, sizeof(result_msg) - rm,
                               " SUNK:%s", sunk_ship_name);
            }

            printf("%s", server_log);

            /* Apply cooldown */
            if (cooldown_for(ship_type) > 0)
                chosen->cooldown = cooldown_for(ship_type);

            /* Notify shooter */
            srv_send(shooter_pid, result_msg);

            /* Notify opponent */
            char opp_msg[MSG_SIZE * 2];
            snprintf(opp_msg, sizeof(opp_msg),
                     "OPP_FIRE %c %s HITS:%d MISSES:%d%s%s",
                     ship_type, coords_str, hits, misses,
                     sunk_ship_name[0] ? " SUNK:" : "",
                     sunk_ship_name[0] ? sunk_ship_name : "");
            srv_send(target_pid, opp_msg);

            return;
        }
        next_input:;
    }
}

/* ─── Turn loop ──────────────────────────────────────────────────────────── */
static void decrement_cooldowns(Player *p) {
    for (int i = 0; i < p->num_ships; i++)
        if (p->ships[i].cooldown > 0)
            p->ships[i].cooldown--;
}

static int all_cooldown(Player *p) {
    for (int i = 0; i < p->num_ships; i++)
        if (!p->ships[i].sunk && p->ships[i].cooldown == 0)
            return 0;
    return 1;
}

static void build_turn_msg(int pid, char *out, size_t osz) {
    Player *p = &players[pid];
    int n = 0;
    n += snprintf(out + n, osz - n, "YOUR_TURN ");
    /* Append own board */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char cell = p->board[r][c];
            /* If this tile was hit by opponent, show X */
            if (p->hit_board[r][c] == CELL_HIT) cell = CELL_HIT;
            out[n++] = cell;
        }
    }
    out[n++] = '|';
    /* Append enemy hit board (what this player knows about opponent) */
    Player *opp = &players[1 - pid];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            out[n++] = opp->hit_board[r][c]; /* X or . */
    out[n++] = '|';
    /* Append cooldown info: "S1:0 D1:2 C1:3 ..." */
    for (int i = 0; i < p->num_ships; i++) {
        Ship *s = &p->ships[i];
        n += snprintf(out + n, osz - n, " %c%d:%d", s->type, s->index, s->cooldown);
    }
    out[n] = '\0';
}

static void run_game(void) {
    srand((unsigned)time(NULL));
    int current = rand() % 2;
    printf("[SERVER] Randomly selected: Player %d goes first.\n", current + 1);

    char turn_msg[MSG_SIZE * 8];
    char wait_msg[MSG_SIZE];

    while (1) {
        int other = 1 - current;

        /* Decrement cooldowns for current player at start of their turn */
        decrement_cooldowns(&players[current]);

        /* Send YOUR_TURN to current player */
        build_turn_msg(current, turn_msg, sizeof(turn_msg));
        /* Append all-cooldown flag */
        if (all_cooldown(&players[current]))
            strncat(turn_msg, " ALL_CD", sizeof(turn_msg) - strlen(turn_msg) - 1);
        srv_send(current, turn_msg);

        /* Send WAIT to the other player */
        snprintf(wait_msg, sizeof(wait_msg),
                 "WAIT Player %d is taking their turn...", current + 1);
        srv_send(other, wait_msg);

        /* Process the fire */
        process_fire(current);

        /* Check win condition */
        if (players[other].ships_remaining == 0) {
            srv_send(current, "YOU_WIN");
            srv_send(other,   "YOU_LOSE");
            printf("[SERVER] Player %d wins!\n", current + 1);
            break;
        }

        current = other;
    }
}

/* ─── Cleanup ────────────────────────────────────────────────────────────── */
static void cleanup(void) {
    mq_close(mq_in[0]);  mq_unlink(MQ_P1_TO_SRV);
    mq_close(mq_in[1]);  mq_unlink(MQ_P2_TO_SRV);
    mq_close(mq_out[0]); mq_unlink(MQ_SRV_TO_P1);
    mq_close(mq_out[1]); mq_unlink(MQ_SRV_TO_P2);
    printf("[SERVER] Cleanup done. Goodbye.\n");
}

/* ─── Main ───────────────────────────────────────────────────────────────── */
int main(void) {
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10,
                            .mq_msgsize = MSG_SIZE, .mq_curmsgs = 0 };

    /* Unlink any stale queues */
    mq_unlink(MQ_BS_JOIN);
    mq_unlink(MQ_P1_TO_SRV); mq_unlink(MQ_P2_TO_SRV);
    mq_unlink(MQ_SRV_TO_P1); mq_unlink(MQ_SRV_TO_P2);

    printf("[SERVER] Battleship Game Master started.\n");

    mq_in[0]  = mq_open(MQ_P1_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
    mq_in[1]  = mq_open(MQ_P2_TO_SRV, O_CREAT | O_RDONLY, 0666, &attr);
    mq_out[0] = mq_open(MQ_SRV_TO_P1, O_CREAT | O_WRONLY, 0666, &attr);
    mq_out[1] = mq_open(MQ_SRV_TO_P2, O_CREAT | O_WRONLY, 0666, &attr);
    
    mqd_t mq_join = mq_open(MQ_BS_JOIN, O_CREAT | O_RDONLY, 0666, &attr);

    if (mq_in[0] == (mqd_t)-1 || mq_in[1] == (mqd_t)-1 ||
        mq_out[0] == (mqd_t)-1 || mq_out[1] == (mqd_t)-1 || mq_join == (mqd_t)-1) {
        perror("mq_open"); cleanup(); return 1;
    }

    memset(players, 0, sizeof(players));
    for (int i = 0; i < 2; i++) init_board(&players[i]);

    /* Wait for both players to send CONNECT via the join queue */
    for (int i = 0; i < 2; i++) {
        char buf[MSG_SIZE];
        printf("[SERVER] Waiting for Player %d...\n", i + 1);
        memset(buf, 0, MSG_SIZE);
        mq_receive(mq_join, buf, MSG_SIZE, NULL);
        
        if (strncmp(buf, "CONNECT ", 8) == 0) {
            char temp_q[128];
            sscanf(buf + 8, "%s", temp_q);
            
            /* Reply with assigned ID */
            mqd_t mq_temp = mq_open(temp_q, O_WRONLY);
            if (mq_temp != (mqd_t)-1) {
                char reply[32];
                snprintf(reply, sizeof(reply), "%d", i + 1);
                mq_send(mq_temp, reply, strlen(reply) + 1, 0);
                mq_close(mq_temp);
            }
            
            players[i].connected = 1;
            printf("[SERVER] Player %d connected.\n", i + 1);
        } else {
            i--; /* Retry if invalid message */
        }
    }
    
    mq_close(mq_join);
    mq_unlink(MQ_BS_JOIN);
    
    printf("[SERVER] Both players connected. Game starting!\n");
    srv_send(0, "GAME_START");
    srv_send(1, "GAME_START");

    /* Fleet setup (concurrent: server handles sequentially, players run in parallel) */
    printf("[SERVER] Player 1 is setting up their fleet...\n");
    setup_player(0);
    printf("[SERVER] Player 2 is setting up their fleet...\n");
    setup_player(1);
    printf("[SERVER] Fleet setup complete.\n");
    srv_send(0, "SETUP_DONE");
    srv_send(1, "SETUP_DONE");

    run_game();
    cleanup();
    return 0;
}
