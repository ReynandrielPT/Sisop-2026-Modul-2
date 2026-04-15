# Task 3 _(Simulasi Pertempuran Laut / Naval Battle Simulation)_

Kenji dan James adalah dua sahabat yang sama-sama gemar mempelajari sejarah Perang Dunia II, khususnya pertempuran laut di Samudra Pasifik antara Angkatan Laut Kekaisaran Jepang dan Angkatan Laut Amerika Serikat. Mereka berdua terinspirasi oleh taktik kapal yang digunakan pada masa itu dan ingin mensimulasikan pengalaman tersebut dalam sebuah permainan.

Untuk mengobati rasa penasaran mereka, Kenji dan James memutuskan untuk membuat sebuah permainan papan sederhana di terminal komputer yang meniru pertempuran laut tersebut. Permainan ini menggunakan konsep **Battleship** pada papan berukuran 4x4 yang dimainkan oleh dua pemain secara bergantian melalui komunikasi antar proses.

Bantu Kenji dan James mewujudkan permainan tersebut!

---

## Deskripsi Permainan _(Game Description)_

Setiap pemain memiliki papan perang berukuran **4x4** yang diindeks dengan baris `0-3` dan kolom `A-D`. Posisi dinyatakan dengan menggabungkan baris dan kolom, misalnya `0A` berarti baris 0 kolom A, dan `3D` berarti baris 3 kolom D.

### Armada _(Fleet)_

Setiap pemain mendapatkan tepat **2 Kapal**. Setiap kapal berukuran **1 petak** (1x1) dan hanya dapat menembak 1 petak per giliran. Tidak ada sistem cooldown. Setiap kapal direpresentasikan dengan simbol `S`.

### Tampilan Papan _(Board Display)_

Setiap giliran, pemain akan melihat papan lawan di bagian atas dan papan miliknya sendiri di bagian bawah (lebih dekat dengan prompt masukan).

```text
    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|X|?|?|
2|?|?| |?|
3| |?|?|?|

    Papan Anda
  A B C D
0|S| | | |
1| | | | |
2| | |S| |
3| | | | |
```

Keterangan:

- `S` : Kapal milik pemain
- `X` : Petak yang tertembak dan kena
- ` ` : Petak yang tertembak tapi kosong (meleset)
- `?` : Petak lawan yang belum ditembak

---

## Soal _(Tasks)_

### a. Persiapan Koneksi dan Antrean Pesan _(Connection Setup and Message Queues)_

Buatlah program `game.c` yang berperan sebagai **Game Master** dan program `player.c` sebagai klien pemain interaktif.

**Server** harus membuat dan mengelola **message queue** untuk komunikasi. Dibutuhkan 1 queue publik untuk menerima koneksi pemain baru dan **4 queue privat** untuk komunikasi dua arah saat gameplay: 2 queue untuk menerima pesan dari setiap pemain, dan 2 queue untuk mengirimkan respons ke setiap pemain.

**Player** dijalankan sederhana tanpa argumen tambahan: `./player`. Program pertama yang terhubung akan ditugaskan sebagai Player 1, dan yang kedua sebagai Player 2.

Ketika pemain pertama bergabung, ia menunggu hingga pemain kedua ikut bergabung. Selama menunggu, program menampilkan pesan seperti berikut:

```text
[SERVER] Menunggu Pemain 2 untuk bergabung...
```

Setelah kedua pemain terhubung, server mengirimkan sinyal bahwa permainan siap dimulai.

### b. Penempatan Armada _(Fleet Placement)_

Setelah kedua pemain terhubung, masing-masing pemain menempatkan 2 kapalnya secara bergantian.

Program akan meminta pemain menempatkan setiap kapal satu per satu dengan memasukkan koordinat 1 petak.

Contoh sesi penempatan:

```text
Tempatkan Kapal 1:
> 0A
Kapal 1 ditempatkan.

Tempatkan Kapal 2:
> 0A
Petak sudah ditempati!

Tempatkan Kapal 2:
> 3D
Kapal 2 ditempatkan.
```

Ketentuan validasi penempatan:

- Koordinat harus berada dalam batas papan (baris `0-3`, kolom `A-D`).
- Petak yang dipilih tidak boleh menimpa kapal yang sudah ditempatkan.

Setelah pemain menyelesaikan penempatan, program menampilkan:

```text
[INFO] Menunggu lawan menyelesaikan penempatan...
```

Permainan baru dimulai setelah kedua pemain selesai menempatkan kapalnya.

### c. Giliran Bermain dan Penembakan _(Turn System and Firing)_

Setelah setup selesai, server menentukan pemain pertama secara acak. Pemain yang mendapat giliran pertama mendapat notifikasi, sedangkan pemain lain menunggu.

Setiap giliran, program menampilkan papan lawan di bagian atas, papan milik pemain di bagian bawah, lalu meminta pemain memilih koordinat tembakan.

```text
========================================
[GILIRAN ANDA]

    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|X|?|?|
2|?|?| |?|
3| |?|?|?|

    Papan Anda
  A B C D
0|S| | | |
1| | | | |
2| | |S| |
3| | | | |

Target:
```

Format perintah tembak hanya membutuhkan koordinat target tembakan.

```text
Target: 2C
```

Setelah tembakan dilakukan, server memproses hasilnya dan mengirimkan notifikasi secara berurutan:

**1. Pemain yang menembak** mendapat informasi di akhir gilirannya:

```text
[HASIL TEMBAKAN]
2C KENA KAPAL, SISA 1 KAPAL LAGI
========================================
```

**2. Pemain lawan** mendapat informasi setelah gilirannya dimulai:

```text
[INFO] Lawan menembak 2C: KENA KAPAL, SISA 1 KAPAL LAGI
```

**3. Terminal server** mencatat seluruh kejadian:

```text
[GILIRAN] Pemain 1 menembak 2C: KENA
[TENGGELAM] Pemain 1 menenggelamkan Kapal Pemain 2!
```

Ketentuan validasi yang dilakukan server:

- Koordinat tembakan harus berada dalam batas papan (baris `0-3`, kolom `A-D`).
- Pemain dapat menembak petak yang sebelumnya sudah pernah ditembak, namun hasilnya selalu dianggap `MELESET`.
- Setiap giliran, pemain hanya dapat menembak satu kali pada satu target koordinat.

### d. Pengelolaan Thread, Akhir Permainan, dan Pembersihan _(Thread Management, End of Game, and Cleanup)_

Program `player.c` harus menggunakan **thread** dan **mutex** untuk mengelola komunikasi secara bersamaan:

1. Thread utama menangani input pemain dan mengirimkan perintah ke server.
2. Thread terpisah mendengarkan pesan masuk dari server secara kontinu, seperti notifikasi tembakan lawan dan pembaruan papan.
3. Sebuah mutex melindungi pembaruan tampilan papan agar tidak terjadi race condition antara kedua thread.

Ketika tembakan lawan mendarat di papan pemain, thread pendengar menampilkan informasi tembakan tersebut secara langsung.

Permainan berakhir ketika semua kapal milik salah satu pemain berhasil ditenggelamkan. Server mengirimkan pesan kemenangan dan kekalahan ke masing-masing pemain.

Pemenang:

```text
========================================
[HASIL] Semua kapal musuh telah tenggelam!
[HASIL] ANDA MENANG!
========================================
```

Yang kalah:

```text
========================================
[HASIL] Semua kapal Anda telah tenggelam.
[HASIL] ANDA KALAH.
========================================
```

Setelah permainan berakhir, kedua program harus melakukan pembersihan semua message queue sebelum keluar.

Jalankan server dengan:

```bash
gcc game.c -o game
./game
```

Jalankan setiap pemain di terminal terpisah dengan:

```bash
gcc player.c -o player -lpthread
./player
```

---

### Contoh Jalannya Program _(Example Run)_

> Jalankan 3 terminal secara bersamaan. Terminal server harus dijalankan lebih dulu.

**Terminal 1 — Server (`./game`)**

```text
[SERVER] Game Master Battleship dimulai (4x4 Sederhana).
[SERVER] Menunggu Pemain 1...
[SERVER] Pemain 1 terhubung.
[SERVER] Menunggu Pemain 2...
[SERVER] Pemain 2 terhubung.
[SERVER] Kedua pemain terhubung. Permainan dimulai!
[SERVER] Pemain 1 sedang menempatkan armada...
[SERVER] Pemain 2 sedang menempatkan armada...
[SERVER] Penempatan armada selesai.
[SERVER] Dipilih secara acak: Pemain 1 bermain duluan.
[GILIRAN] Pemain 1 menembak 0A: KENA
[GILIRAN] Pemain 2 menembak 1B: MELESET
[GILIRAN] Pemain 1 menembak 3D: KENA
[TENGGELAM] Pemain 1 menenggelamkan Kapal Pemain 2!
...
[SERVER] Pemain 1 menang!
[SERVER] Pembersihan selesai. Sampai jumpa.
```

**Terminal 2 — Pemain 1 (`./player`)**

```text
[PEMAIN] Menghubungkan ke server...
[PEMAIN 1] Berhasil terhubung!
[SERVER] Menunggu Pemain 2 untuk bergabung...
[SERVER] Permainan dimulai (4x4 Sederhana)!

    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|?|?|?|
2|?|?|?|?|
3|?|?|?|?|

    Papan Anda
  A B C D
0| | | | |
1| | | | |
2| | | | |
3| | | | |

Anda akan menempatkan 2 kapal (masing-masing 1 petak).

Tempatkan Kapal 1:
> 0A
Kapal 1 ditempatkan.
Tempatkan Kapal 2:
> 2B
Kapal 2 ditempatkan.

[INFO] Menunggu lawan menyelesaikan penempatan...
[INFO] Semua pemain siap.

========================================
[GILIRAN ANDA]

    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|?|?|?|
2|?|?|?|?|
3|?|?|?|?|

    Papan Anda
  A B C D
0|S| | | |
1| | | | |
2| |S| | |
3| | | | |

Target: 1A

[HASIL TEMBAKAN]
1A MELESET, SISA 2 KAPAL LAGI
========================================
...
[HASIL] Semua kapal musuh telah tenggelam!
[HASIL] ANDA MENANG!
```

---

# Task 3 _(Naval Battle Simulation)_

Kenji and James are two close friends who share a passion for studying the history of World War II, particularly the naval battles fought across the Pacific Ocean between the Imperial Japanese Navy and the United States Navy. Both of them have always been fascinated by the naval tactics during that era and wish to simulate that experience through a game.

To satisfy their curiosity, Kenji and James decide to build a simple terminal-based board game that emulates those naval engagements. The game is based on the classic **Battleship** concept, played on a 4x4 grid, with two players taking turns communicating through inter-process mechanisms.

Help Kenji and James bring this game to life!

---

## Game Description

Each player owns a **4x4** battle grid indexed by rows `0-3` and columns `A-D`. A position is written by combining the row and column into a single token, for example `0A` means row 0 column A, and `3D` means row 3 column D.

### Fleet Composition

Each player automatically gets exactly **2 Ships**. Each ship is exactly **1 tile** in size (1x1) and can only fire at 1 tile per turn. There are no cooldown mechanics. Each ship is represented by the symbol `S`.

### Board Display

Each turn, the active player sees the **enemy board on top** and their **own board below** (closer to the input prompt).

```text
    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|X|?|?|
2|?|?| |?|
3| |?|?|?|

    Papan Anda
  A B C D
0|S| | | |
1| | | | |
2| | |S| |
3| | | | |
```

Legend:

- `S` : Player's Ship
- `X` : Tile that was shot and hit
- ` ` : Tile that was shot but empty
- `?` : Unknown enemy tile (not yet targeted)

---

## Tasks

### a. Connection Setup and Message Queues

Create a `game.c` program that acts as the **Game Master** and a `player.c` program as the interactive player client.

The **server** must create and manage **message queues** for communication. This includes 1 public queue to accept player connections and **4 private queues** for bidirectional gameplay communication: 2 queues to receive messages from each player, and 2 queues to send responses back to each player.

To launch the **Player** client, simply run `./player` without arguments. The first process to connect successfully will be automatically assigned as Player 1, and the second as Player 2.

When the first player connects, they wait until the second player joins. During this wait the program displays:

```text
[SERVER] Menunggu Pemain 2 untuk bergabung...
```

Once both players are connected, the server signals that the game is ready to begin.

### b. Fleet Placement

After both players are connected, each player places their 2 ships.

The program prompts the player to place each ship one at a time by entering a single tile coordinate.

Example placement session:

```text
Tempatkan Kapal 1:
> 0A
Kapal 1 ditempatkan.

Tempatkan Kapal 2:
> 0A
Petak sudah ditempati!

Tempatkan Kapal 2:
> 3D
Kapal 2 ditempatkan.
```

Placement validation rules:

- Coordinates must be within the grid (rows `0-3`, columns `A-D`).
- No tile may overlap a ship already placed.

Once a player finishes placing all ships, the program displays:

```text
[INFO] Menunggu lawan menyelesaikan penempatan...
```

The game starts only after both players have completed their fleet setup.

### c. Turn System and Firing

After setup, the server randomly determines which player takes the first turn. The chosen player receives a start notification while the other waits.

Each turn, the program displays the **enemy board on top**, the **player's own board below**, and then prompts the player to target a coordinate.

```text
========================================
[GILIRAN ANDA]

    Papan Lawan
  A B C D
0|?|?|?|?|
1|?|X|?|?|
2|?|?| |?|
3| |?|?|?|

    Papan Anda
  A B C D
0|S| | | |
1| | | | |
2| | |S| |
3| | | | |

Target:
```

The player enters exactly 1 coordinate to strike.

```text
Target: 2C
```

After the shot is fired, the server processes the results and sends notifications:

**1. The shooting player** receives the result at the end of their turn:

```text
[HASIL TEMBAKAN]
2C KENA KAPAL, SISA 1 KAPAL LAGI
========================================
```

**2. The opponent** is informed at the start of the opponent's turn:

```text
[INFO] Lawan menembak 2C: KENA KAPAL, SISA 1 KAPAL LAGI
```

**3. The server terminal** logs the event:

```text
[GILIRAN] Pemain 1 menembak 2C: KENA
[TENGGELAM] Pemain 1 menenggelamkan Kapal Pemain 2!
```

Validation rules enforced by the server:

- Target coordinates must be within the grid (rows `0-3`, columns `A-D`).
- Players may shoot at tiles that have already been targeted before, but the result is always treated as a `MISS`.
- Each turn allows only one target coordinate.

### d. Thread Management, End of Game, and Cleanup

The `player.c` program must use **threads** and **mutex** for concurrent communication:

1. The main thread handles player input and sends commands to the server.
2. A separate thread continuously listens for incoming messages from the server, such as opponent shot notifications and board updates.
3. A mutex protects board display updates to prevent race conditions between the two threads.

When an opponent's shot lands on the player's grid, the listener thread immediately displays the event.

The game ends when all ships belonging to one player have been sunk. The server sends the appropriate message to each player.

Winner:

```text
========================================
[HASIL] Semua kapal musuh telah tenggelam!
[HASIL] ANDA MENANG!
========================================
```

Loser:

```text
========================================
[HASIL] Semua kapal Anda telah tenggelam.
[HASIL] ANDA KALAH.
========================================
```

After the game ends, both programs must clean up all message queues before exiting.

Run the server with:

```bash
gcc game.c -o game
./game
```

Run each player in a separate terminal with:

```bash
gcc player.c -o player -lpthread
./player
```


