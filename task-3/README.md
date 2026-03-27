# Task 3 _(Simulasi Pertempuran Laut / Naval Battle Simulation)_

Kenji dan James adalah dua sahabat yang sama-sama gemar mempelajari sejarah Perang Dunia II, khususnya pertempuran laut di Samudra Pasifik antara Angkatan Laut Kekaisaran Jepang dan Angkatan Laut Amerika Serikat. Mereka berdua terinspirasi oleh taktik kapal selam, kapal perusak, dan kapal penjelajah yang digunakan pada masa itu dan ingin mensimulasikan pengalaman tersebut dalam sebuah permainan.

Untuk mengobati rasa penasaran mereka, Kenji dan James memutuskan untuk membuat sebuah permainan papan sederhana di terminal komputer yang meniru pertempuran laut tersebut. Permainan ini menggunakan konsep **Battleship** pada papan berukuran 5x5 yang dimainkan oleh dua pemain secara bergantian melalui komunikasi antar proses.

Bantu Kenji dan James mewujudkan permainan tersebut!

---

## Deskripsi Permainan _(Game Description)_

Setiap pemain memiliki papan perang berukuran **5x5** yang diindeks dengan baris `0-4` dan kolom `A-E`. Posisi dinyatakan dengan menggabungkan baris dan kolom, misalnya `0A` berarti baris 0 kolom A, dan `3D` berarti baris 3 kolom D.

### Jenis Kapal _(Ship Types)_

| Jenis Kapal | Simbol | Ukuran | Jumlah Tile Tembakan | Cooldown | Poin Armada |
| :---: | :---: | :---: | :---: | :---: | :---: |
| Kapal Selam _(Submarine)_ | `S` | 2 petak | 1 petak | Tidak ada | 2 |
| Kapal Perusak _(Destroyer)_ | `D` | 2 petak | 2 petak (bebas posisi) | 2 giliran | 2 |
| Kapal Penjelajah _(Cruiser)_ | `C` | 3 petak | 3 petak (bebas posisi) | 3 giliran | 3 |

### Armada _(Fleet)_

Setiap pemain memiliki **batas maksimum poin armada sebesar 7**. Pemain bebas memilih kombinasi kapal selama total poin tidak melebihi batas tersebut. Pemain dapat membawa hanya 1 kapal jika diinginkan. Setiap kapal ditempatkan secara horizontal atau vertikal dan tidak boleh tumpang tindih maupun keluar dari batas papan.

| Kombinasi | Total Poin |
| :---: | :---: |
| S+S+S | 6 |
| D+D+D | 6 |
| C+C | 6 |
| S+D+C | 7 |
| S+S+C | 7 |
| Dan kombinasi lain yang tidak melebihi 7 poin | |

> **Catatan:** Setiap giliran, pemain hanya dapat menembak satu kali menggunakan satu kapal pilihannya, meskipun memiliki lebih dari satu kapal yang siap menembak.

### Tampilan Papan _(Board Display)_

Setiap giliran, pemain akan melihat papan lawan di bagian atas dan papan miliknya sendiri di bagian bawah (lebih dekat dengan prompt masukan).

```
    Papan Lawan (Enemy Board)
  A B C D E
0|?|?|?|?|?|
1|?|X|?|?|?|
2|?|?| |?|?|
3| |?|?|?|?|
4|?|?|?|?|?|

    Papan Anda (Your Board)
  A B C D E
0|S|S| | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |
```

Keterangan:
- `S` : Kapal Selam milik pemain
- `D` : Kapal Perusak milik pemain
- `C` : Kapal Penjelajah milik pemain
- `X` : Tembakan mengenai sasaran (hit)
- `~` : Tembakan musuh (maupun pemain sendiri) meleset (miss) jatuh di laut
- ` ` : Petak kosong yang belum tertembak
- `?` : Petak lawan yang belum ditembak (unknown)

---

## Soal _(Tasks)_

### a. Persiapan Koneksi dan Antrean Pesan _(Connection Setup and Message Queues)_

Buatlah program `server.c` yang berperan sebagai **Game Master** dan program `player.c` sebagai klien pemain interaktif.

**Server** harus membuat dan mengelola **4 POSIX Message Queue** untuk komunikasi dua arah antara server dan masing-masing pemain: 2 queue untuk menerima pesan dari setiap pemain, dan 2 queue untuk mengirimkan respons ke setiap pemain.

**Player** dijalankan sederhana tanpa argumen tambahan: `./player`. Program pertama yang terhubung akan ditugaskan sebagai Player 1, dan yang kedua sebagai Player 2.

Ketika pemain pertama bergabung, ia menunggu hingga pemain kedua ikut bergabung. Selama menunggu, program menampilkan pesan seperti berikut:

```
[SERVER] Waiting for Player 2 to connect...
```

Setelah kedua pemain terhubung, server mengirimkan sinyal bahwa permainan siap dimulai dan setiap pemain menampilkan papan awalnya yang masih kosong.

### b. Pemilihan dan Penempatan Armada _(Fleet Selection and Placement)_

Setelah kedua pemain terhubung, masing-masing pemain diminta memilih komposisi armadanya dan menempatkan kapal-kapalnya secara terpisah dan bersamaan.

**Langkah 1: Memilih komposisi armada**

Program menampilkan tabel jenis kapal beserta poinnya, kemudian meminta pemain mengetikkan jumlah masing-masing kapal. Format masukan adalah jumlah diikuti simbol kapal, dipisahkan spasi. Contoh:

```
Pilih armada Anda (Maks. 7 poin):
  S (Submarine)  - 2 pts  | 2 petak | tembak 1 petak | cooldown: tidak ada
  D (Destroyer)  - 2 pts  | 2 petak | tembak 2 petak | cooldown: 2 giliran
  C (Cruiser)    - 3 pts  | 3 petak | tembak 3 petak | cooldown: 3 giliran

Ketik pilihan Anda (contoh: 2S 1D  atau  1S 1D 1C): 1S 2D
```

Jika total poin melebihi 7 atau masukan tidak valid, program meminta pemain mengulangi pilihan.

**Langkah 2: Menempatkan kapal**

Setelah komposisi dikonfirmasi, program meminta pemain menempatkan setiap kapal satu per satu. Pemain cukup memasukkan **2 koordinat tepi** kapal (awal dan akhir), dipisahkan spasi. Program secara otomatis mengisi petak-petak di antara keduanya.

Jika pemain hanya memiliki satu kapal dari jenis tertentu, prompt menggunakan nama kapal saja. Jika memiliki lebih dari satu kapal dari jenis yang sama, prompt menambahkan nomor urut.

Contoh sesi penempatan (armada: 2 Cruiser):

```
Place Cruiser 1
3C 3E
Cruiser 1 placed.

Place Cruiser 2
3C 3C
Please provide two different edge coordinates!

Place Cruiser 2
3A 3C
Tiles are already occupied!

Place Cruiser 2
3A 3B
Cruiser needs exactly 3 adjacent tiles!

Place Cruiser 2
3A 0A
Too many tiles!

Place Cruiser 2
2A 4B
The tiles are not adjacent!

Place Cruiser 2
2C 3E
Too many tiles!

Place Cruiser 2
2A 4A
Cruiser 2 placed.
```

Ketentuan validasi penempatan:
- Kedua koordinat harus berbeda dan berada dalam batas papan (baris `0-4`, kolom `A-E`).
- Kedua koordinat tepi harus berada pada **baris yang sama** (horizontal) atau **kolom yang sama** (vertikal). Penempatan diagonal tidak diperbolehkan.
- Jarak antara kedua koordinat tepi harus tepat sesuai ukuran kapal (Submarine: 2 petak, Destroyer: 2 petak, Cruiser: 3 petak).
- Semua petak yang dilalui kapal tidak boleh menimpa kapal yang sudah ditempatkan.

Setelah pemain menyelesaikan penempatan, program menampilkan:

```
[INFO] Menunggu pemain lain menyelesaikan penempatan kapal...
```

Permainan baru dimulai setelah kedua pemain selesai menempatkan seluruh armadanya.

### c. Giliran Bermain dan Penembakan _(Turn System and Firing)_

Setelah setup selesai, server menentukan pemain pertama secara acak. Pemain yang mendapat giliran pertama mendapat notifikasi, sedangkan pemain lain menunggu.

Setiap giliran, program menampilkan papan lawan di bagian atas, papan milik pemain di bagian bawah, status cooldown setiap kapal, lalu meminta pemain memilih kapal dan koordinat tembakan.

```
========================================
[GILIRAN ANDA]

    Papan Lawan (Enemy Board)
  A B C D E
0|?|?|?|?|?|
1|?|X|?|?|?|
2|?|?| |?|?|
3| |?|?|?|?|
4|?|?|?|?|?|

    Papan Anda (Your Board)
  A B C D E
0|S|S| | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |

Status kapal:
  S (Submarine) : SIAP (tidak ada cooldown)
  D (Destroyer) : COOLDOWN (1 giliran lagi)
  C (Cruiser)   : SIAP

Pilih kapal (S/C): 
```

Format perintah tembak menggunakan simbol kapal diikuti koordinat tembakan.

**Kapal Selam (S) - 1 petak:**
```
Pilih kapal: S
Koordinat (1 petak): 2C
```

**Kapal Perusak (D) - 2 petak:**
```
Pilih kapal: D
Koordinat (2 petak): 1A 4E
```

**Kapal Penjelajah (C) - 3 petak:**
```
Pilih kapal: C
Koordinat (3 petak): 0B 3D 4A
```

Setelah tembakan dilakukan, server memproses hasilnya dan mengirimkan notifikasi secara berurutan:

**1. Pemain yang menembak** mendapat informasi di akhir gilirannya:

```
[HASIL TEMBAKAN]
  - 1A: KENA (HIT)
  - 4E: MELESET (MISS)
  1 kena, 1 meleset.
  Kapal Selam lawan telah ditenggelamkan!
```

**2. Pemain lawan** mendapat informasi setelah gilirannya dimulai:

```
[INFO] Lawan menembak menggunakan Destroyer di 1A dan 4E: 1 kena, 1 meleset.
  Kapal Selam Anda telah ditenggelamkan!
```

**3. Terminal server** mencatat seluruh kejadian:

```
[TURN] Player 1 fired using Destroyer at 1A, 4E
  - 1A: HIT
  - 4E: MISS
  Result: 1 hit(s), 1 miss(es)
  [SUNK] Player 1 sank Player 2's Submarine!
```

Ketentuan validasi yang dilakukan server:
- Kapal yang sedang dalam cooldown tidak dapat dipilih. Jika dipilih, program menampilkan pesan cooldown dan meminta pemain memilih kapal lain.
- Koordinat tembakan harus berada dalam batas papan (baris `0-4`, kolom `A-E`).
- Untuk Kapal Perusak (2 petak) dan Kapal Penjelajah (3 petak), setiap koordinat tembakan dapat berada di mana saja pada papan dan tidak perlu bersebelahan satu sama lain.
- Pemain dapat menembak petak yang sebelumnya sudah pernah ditembak.
- Setiap giliran, pemain hanya dapat menembak satu kali.

**Kasus khusus - Semua kapal dalam cooldown:**

Jika seluruh kapal pemain sedang dalam kondisi cooldown, program menampilkan kedua papan, menampilkan pesan bahwa semua kapal sedang dalam cooldown, dan secara otomatis melewati giliran pemain tersebut.

```
========================================
[GILIRAN ANDA]

    Papan Lawan (Enemy Board)
  A B C D E
0|?|X|?|?|?|
1|?|?|?|?|?|
2|?|?| |?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Papan Anda (Your Board)
  A B C D E
0| | | | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |

Status kapal:
  D (Destroyer) : COOLDOWN (2 giliran lagi)
  C (Cruiser)   : COOLDOWN (1 giliran lagi)

[INFO] Semua kapal Anda sedang dalam cooldown. Giliran dilewati secara otomatis.
========================================
```

### d. Pengelolaan Thread, Akhir Permainan, dan Pembersihan _(Thread Management, End of Game, and Cleanup)_

Program `player.c` harus menggunakan **thread** dan **mutex** untuk mengelola komunikasi secara bersamaan:

1. Thread utama menangani input pemain dan mengirimkan perintah ke server.
2. Thread terpisah mendengarkan pesan masuk dari server secara kontinu, seperti notifikasi tembakan lawan dan pembaruan papan.
3. Sebuah mutex melindungi pembaruan tampilan papan agar tidak terjadi race condition antara kedua thread.

Ketika tembakan lawan mendarat di papan pemain, thread pendengar menampilkan informasi tembakan tersebut secara langsung.

Permainan berakhir ketika semua kapal milik salah satu pemain berhasil ditenggelamkan. Server mengirimkan pesan kemenangan dan kekalahan ke masing-masing pemain.

```
========================================
[HASIL] Kapal perusak lawan telah ditenggelamkan!
[HASIL] Semua kapal lawan telah ditenggelamkan!
[HASIL] ANDA MENANG!
========================================
```

Setelah permainan berakhir, kedua program harus melakukan pembersihan semua message queue sebelum keluar.

Jalankan server dengan:
```bash
gcc server.c -o server -lrt
./server
```

Jalankan setiap pemain di terminal terpisah dengan:
```bash
gcc player.c -o player -lrt -lpthread
./player
```

---

### Contoh Jalannya Program _(Example Run)_

> Jalankan 3 terminal secara bersamaan. Terminal server harus dijalankan lebih dulu.

**Terminal 1 — Server (`./server`)**
```
[SERVER] Battleship Game Master started.
[SERVER] Waiting for Player 1...
[SERVER] Player 1 connected.
[SERVER] Waiting for Player 2...
[SERVER] Player 2 connected.
[SERVER] Both players connected. Game starting!
[SERVER] Player 1 is setting up their fleet...
[SERVER] Player 2 is setting up their fleet...
[SERVER] Fleet setup complete.
[SERVER] Randomly selected: Player 1 goes first.
[TURN] Player 1 fired using Destroyer at 0A, 3E
  - 0A: HIT
  - 3E: MISS
  Result: 1 hit(s), 1 miss(es)
[TURN] Player 2 fired using Submarine at 1B
  - 1B: MISS
  Result: 0 hit(s), 1 miss(es)
[TURN] Player 1 fired using Destroyer at 0B, 2D
  - 0B: HIT
  - 2D: MISS
  Result: 1 hit(s), 1 miss(es)
  [SUNK] Player 1 sank Player 2's Submarine!
[SERVER] Player 2 has no ships remaining. Player 1 wins!
[SERVER] Cleaning up message queues. Goodbye.
```

**Terminal 2 — Player 1 (`./player.sh 1`)**
```
[PLAYER 1] Connecting to server...
[SERVER] Waiting for Player 2 to connect...
[SERVER] Game starting!

Select your fleet (Max 7 points):
  S (Submarine) - 2 pts | 2 tiles | fires 1 tile  | cooldown: none
  D (Destroyer) - 2 pts | 2 tiles | fires 2 tiles | cooldown: 2 turns
  C (Cruiser)   - 3 pts | 3 tiles | fires 3 tiles | cooldown: 3 turns

Enter your selection (example: 2S 1D or 1S 1D 1C): 1D 1C
Fleet confirmed: 1 Destroyer (2 pts) + 1 Cruiser (3 pts) = 5 pts

Place Destroyer
0A 0B
Destroyer placed.

Place Cruiser
2C 2E
Cruiser placed.

[INFO] Waiting for Player 2 to finish placement...
[INFO] All players ready. Player 1 goes first!

========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|?|?|?|?|
2|?|?|?|?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|D|D| | | |
1| | | | | |
2| | |C|C|C|
3| | | | | |
4| | | | | |

Ship status:
  D (Destroyer) : READY
  C (Cruiser)   : READY

Select a ship (D/C): D
Target (2 tiles): 0A 3E

[SHOT RESULT]
  - 0A: HIT
  - 3E: MISS
  1 hit(s), 1 miss(es).
========================================
...

[RESULT] You sank the enemy's Submarine!
[RESULT] All enemy ships have been sunk!
[RESULT] YOU WIN!
```

**Terminal 3 — Player 2 (`./player.sh 2`)**
```
[PLAYER 2] Connecting to server...
[SERVER] Game starting!

Enter your selection (example: 2S 1D or 1S 1D 1C): 2S
Fleet confirmed: 2 Submarines (2+2 = 4 pts)

Place Submarine 1
0A 0B
Submarine 1 placed.

Place Submarine 2
2D 3D
Submarine 2 placed.

[INFO] Waiting for Player 1 to finish placement...
[INFO] Player 1 goes first. Waiting for opponent's turn...
[INFO] Opponent fired using Destroyer at 0A and 3E: 1 hit(s), 1 miss(es).
  Your Submarine 1 has been hit!
========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|?|?|?|?|
2|?|?|?|?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|X|S| | | |
1| | | | | |
2| | | |S| |
3| | | |S| |
4| | | | | |

Ship status:
  S1 (Submarine 1) : READY
  S2 (Submarine 2) : READY

Select a ship (S1/S2): S1
Target (1 tile): 1B
...

[RESULT] All your ships have been sunk.
[RESULT] YOU LOSE.
```

---

---

# Task 3 _(Naval Battle Simulation)_

Kenji and James are two close friends who share a passion for studying the history of World War II, particularly the naval battles fought across the Pacific Ocean between the Imperial Japanese Navy and the United States Navy. Both of them have always been fascinated by the tactics employed by submarines, destroyers, and cruisers during that era and wish to simulate that experience through a game.

To satisfy their curiosity, Kenji and James decide to build a simple terminal-based board game that emulates those naval engagements. The game is based on the classic **Battleship** concept, played on a 5x5 grid, with two players taking turns communicating through inter-process mechanisms.

Help Kenji and James bring this game to life!

---

## Game Description

Each player owns a **5x5** battle grid indexed by rows `0-4` and columns `A-E`. A position is written by combining the row and column into a single token, for example `0A` means row 0 column A, and `3D` means row 3 column D.

### Ship Types

| Ship | Symbol | Size | Tiles Fired per Action | Cooldown | Fleet Points |
| :---: | :---: | :---: | :---: | :---: | :---: |
| Submarine | `S` | 2 tiles | 1 tile | None | 2 |
| Destroyer | `D` | 2 tiles | 2 tiles (any position) | 2 turns | 2 |
| Cruiser | `C` | 3 tiles | 3 tiles (any position) | 3 turns | 3 |

### Fleet Composition

Each player has a **maximum fleet point budget of 7**. Players may freely choose any combination of ships as long as the total points do not exceed that limit. A player may bring as few as 1 ship if desired. Each ship must be placed horizontally or vertically without overlapping or going out of bounds.

| Combination | Total Points |
| :---: | :---: |
| S+S+S | 6 |
| D+D+D | 6 |
| C+C | 6 |
| S+D+C | 7 |
| S+S+C | 7 |
| Any other combination not exceeding 7 points | |

> **Note:** Each turn, a player may fire only once using one ship of their choice, even if multiple ships are ready to fire.

### Board Display

Each turn, the active player sees the **enemy board on top** and their **own board below** (closer to the input prompt), along with each ship's cooldown status.

```
    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|X|?|?|?|
2|?|?| |?|?|
3| |?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|S|S| | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |
```

Legend:
- `S` : Player's Submarine
- `D` : Player's Destroyer
- `C` : Player's Cruiser
- `X` : Confirmed hit
- ` ` : Empty tile or a missed shot (no mark left)
- `?` : Unknown enemy tile (not yet targeted)

---

## Tasks

### a. Connection Setup and Message Queues

Create a `server.c` program that acts as the **Game Master** and a `player.c` program as the interactive player client.

The **server** must create and manage **4 POSIX Message Queues** for bidirectional communication: 2 queues to receive messages from each player, and 2 queues to send responses back to each player.

To launch the **Player** client, simply run `./player` without arguments. The first process to connect successfully will be automatically assigned as Player 1, and the second as Player 2.

When the first player connects, they wait until the second player joins. During this wait the program displays:

```
[SERVER] Waiting for Player 2 to connect...
```

Once both players are connected, the server signals that the game is ready to begin and each player sees their initial empty grid.

### b. Fleet Selection and Placement

After both players are connected, each player independently and concurrently selects their fleet and places their ships.

**Step 1: Selecting fleet composition**

The program displays the ship table with point values and prompts the player to type their desired fleet. The input format is the quantity followed by the ship symbol, separated by spaces. Example:

```
Select your fleet (Max 7 points):
  S (Submarine) - 2 pts  | 2 tiles | fires 1 tile  | cooldown: none
  D (Destroyer) - 2 pts  | 2 tiles | fires 2 tiles | cooldown: 2 turns
  C (Cruiser)   - 3 pts  | 3 tiles | fires 3 tiles | cooldown: 3 turns

Enter your selection (example: 2S 1D  or  1S 1D 1C): 1S 2D
```

If the total points exceed 7 or the input is invalid, the program asks the player to try again.

**Step 2: Placing ships**

After the fleet is confirmed, the program prompts the player to place each ship one at a time. The player only needs to enter **2 edge coordinates** (start and end tile), separated by a space. The program automatically fills all tiles in between.

If the player has only one ship of a given type, the prompt uses just the ship name. If they have more than one ship of the same type, a number is appended.

Example placement session (fleet: 2 Cruisers):

```
Place Cruiser 1
3C 3E
Cruiser 1 placed.

Place Cruiser 2
3C 3C
Please provide two different edge coordinates!

Place Cruiser 2
3A 3C
Tiles are already occupied!

Place Cruiser 2
3A 3B
Cruiser needs exactly 3 adjacent tiles!

Place Cruiser 2
3A 0A
Coordinate out of bounds!

Place Cruiser 2
2A 4B
The tiles are not adjacent!

Place Cruiser 2
2C 3E
Too many tiles!

Place Cruiser 2
2A 4A
Cruiser 2 placed.
```

Placement validation rules:
- Both coordinates must be different and within the grid (rows `0-4`, columns `A-E`).
- Both edge coordinates must share the **same row** (horizontal) or the **same column** (vertical). Diagonal placement is not allowed.
- The span between the two edge coordinates must exactly match the ship size (Submarine: 2 tiles, Destroyer: 2 tiles, Cruiser: 3 tiles).
- No tile in the ship's span may overlap a ship already placed.

Once a player finishes placing all ships, the program displays:

```
[INFO] Waiting for the other player to finish placement...
```

The game starts only after both players have completed their fleet setup.

### c. Turn System and Firing

After setup, the server randomly determines which player takes the first turn. The chosen player receives a start notification while the other waits.

Each turn, the program displays the **enemy board on top**, the **player's own board below**, each ship's cooldown status, and then prompts the player to select a ship and target coordinates.

```
========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|X|?|?|?|
2|?|?| |?|?|
3| |?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|S|S| | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |

Ship status:
  S (Submarine) : READY (no cooldown)
  D (Destroyer) : COOLDOWN (1 turn remaining)
  C (Cruiser)   : READY

Select a ship (S/C):
```

The fire command uses the ship symbol followed by the target coordinates.

**Submarine (S) - 1 tile:**
```
Select ship: S
Target (1 tile): 2C
```

**Destroyer (D) - 2 tiles:**
```
Select ship: D
Target (2 tiles): 1A 4E
```

**Cruiser (C) - 3 tiles:**
```
Select ship: C
Target (3 tiles): 0B 3D 4A
```

After the shot is fired, the server processes the results and sends notifications in sequence:

**1. The shooting player** receives the result at the end of their turn:

```
[SHOT RESULT]
  - 1A: HIT
  - 4E: MISS
  1 hit(s), 1 miss(es).
  You sank the enemy's Submarine!
```

**2. The opponent** is informed at the start of the opponent's turn:

```
[INFO] Opponent fired using Destroyer at 1A and 4E: 1 hit(s), 1 miss(es).
  Your Submarine has been sunk!
```

**3. The server terminal** logs the entire event:

```
[TURN] Player 1 fired using Destroyer at 1A, 4E
  - 1A: HIT
  - 4E: MISS
  Result: 1 hit(s), 1 miss(es)
  [SUNK] Player 1 sank Player 2's Submarine!
```

Validation rules enforced by the server:
- A ship on cooldown cannot be selected. If selected, the program displays a cooldown message and prompts the player to choose another ship.
- All target coordinates must be within the grid (rows `0-4`, columns `A-E`).
- For Destroyers (2 tiles) and Cruisers (3 tiles), each target coordinate may be anywhere on the grid and does not need to be adjacent to the others.
- Players may shoot at tiles that have already been targeted before.
- Each turn allows only one fire action.

**Special case - All ships on cooldown:**

If all of the player's ships are currently on cooldown, the program displays both grids, shows the cooldown status, and automatically skips the player's turn.

```
========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|X|?|?|?|
1|?|?|?|?|?|
2|?|?| |?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0| | | | | |
1| | |D| | |
2| | |D| | |
3|C|C|C| | |
4| | | | | |

Ship status:
  D (Destroyer) : COOLDOWN (2 turns remaining)
  C (Cruiser)   : COOLDOWN (1 turn remaining)

[INFO] All of your ships are on cooldown. Your turn is automatically skipped.
========================================
```

### d. Thread Management, End of Game, and Cleanup

The `player.c` program must use **threads** and **mutex** for concurrent communication:

1. The main thread handles player input and sends commands to the server.
2. A separate thread continuously listens for incoming messages from the server, such as opponent shot notifications and board updates.
3. A mutex protects board display updates to prevent race conditions between the two threads.

When an opponent's shot lands on the player's grid, the listener thread immediately displays the hit information.

The game ends when all ships belonging to one player have been sunk. The server sends the win and lose messages to the respective players.

```
========================================
[RESULT] Enemy destroyer has been sunk!
[RESULT] All enemy ships have been sunk!
[RESULT] YOU WIN!
========================================
```

After the game ends, both programs must clean up all message queues before exiting.

Run the server with:
```bash
gcc server.c -o server -lrt
./server
```

Run each player in a separate terminal with:
```bash
gcc player.c -o player -lrt -lpthread
./player
```

---

### Example Run

> Open 3 terminals. The server must be started first.

**Terminal 1 — Server (`./server`)**
```
[SERVER] Battleship Game Master started.
[SERVER] Waiting for Player 1...
[SERVER] Player 1 connected.
[SERVER] Waiting for Player 2...
[SERVER] Player 2 connected.
[SERVER] Both players connected. Game starting!
[SERVER] Player 1 is setting up their fleet...
[SERVER] Player 2 is setting up their fleet...
[SERVER] Fleet setup complete.
[SERVER] Randomly selected: Player 1 goes first.
[TURN] Player 1 fired using Destroyer at 0A, 3E
  - 0A: HIT
  - 3E: MISS
  Result: 1 hit(s), 1 miss(es)
[TURN] Player 2 fired using Submarine at 1B
  - 1B: MISS
  Result: 0 hit(s), 1 miss(es)
[TURN] Player 1 fired using Destroyer at 0B, 2D
  - 0B: HIT
  - 2D: MISS
  Result: 1 hit(s), 1 miss(es)
  [SUNK] Player 1 sank Player 2's Submarine!
[SERVER] Player 2 has no ships remaining. Player 1 wins!
[SERVER] Cleaning up message queues. Goodbye.
```

**Terminal 2 — Player 1 (`./player.sh 1`)**
```
[PLAYER 1] Connecting to server...
[SERVER] Waiting for Player 2 to connect...
[SERVER] Game starting!

Select your fleet (Max 7 points):
  S (Submarine) - 2 pts | 2 tiles | fires 1 tile  | cooldown: none
  D (Destroyer) - 2 pts | 2 tiles | fires 2 tiles | cooldown: 2 turns
  C (Cruiser)   - 3 pts | 3 tiles | fires 3 tiles | cooldown: 3 turns

Enter your selection (example: 2S 1D or 1S 1D 1C): 1D 1C
Fleet confirmed: 1 Destroyer (2 pts) + 1 Cruiser (3 pts) = 5 pts

Place Destroyer
0A 0B
Destroyer placed.

Place Cruiser
2C 2E
Cruiser placed.

[INFO] Waiting for Player 2 to finish placement...
[INFO] All players ready. Player 1 goes first!

========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|?|?|?|?|
2|?|?|?|?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|D|D| | | |
1| |~| | | |
2| | |C|C|C|
3| | | | | |
4| | | | | |

Ship status:
  D (Destroyer) : READY
  C (Cruiser)   : READY

Select a ship (e.g., S1, D1): D1 0A 3E 4A
Jumlah shot terlalu banyak!
Pilih kapal / Select a ship (e.g., S1, D1): D1
Target 1 (2 shot(s) remaining): 0A
Target 2 (1 shot(s) remaining): 3E

[SHOT RESULT]
  - 0A: HIT
  - 3E: MISS
  1 hit(s), 1 miss(es).
========================================
...

[RESULT] You sank the enemy's Submarine!
[RESULT] All enemy ships have been sunk!
[RESULT] YOU WIN!
```

**Terminal 3 — Player 2 (`./player.sh 2`)**
```
[PLAYER 2] Connecting to server...
[SERVER] Game starting!

Enter your selection (example: 2S 1D or 1S 1D 1C): 2S
Fleet confirmed: 2 Submarines (2+2 = 4 pts)

Place Submarine 1
0A 0B
Submarine 1 placed.

Place Submarine 2
2D 3D
Submarine 2 placed.

[INFO] Waiting for Player 1 to finish placement...
[INFO] Player 1 goes first. Waiting for opponent's turn...
[INFO] Opponent fired using Destroyer at 0A and 3E: 1 hit(s), 1 miss(es).
  Your Submarine 1 has been hit!
========================================
[YOUR TURN]

    Enemy Board
  A B C D E
0|?|?|?|?|?|
1|?|?|?|?|?|
2|?|?|?|?|?|
3|?|?|?|?|?|
4|?|?|?|?|?|

    Your Board
  A B C D E
0|X|S| | | |
1| | | | | |
2| | | |S| |
3| | | |S|~|
4| | | | | |

Ship status:
  S1 (Submarine 1) : READY
  S2 (Submarine 2) : READY

Select a ship (e.g., S1, D1): S1 1B
...

[RESULT] All your ships have been sunk.
[RESULT] YOU LOSE.
```
