# Task 2 (DEANND'S Adventure)

Dono, Enzy, Aini, Nichsaw, Nidji, dan Dodot (DEANND) adalah segerombolan mahasiswa yang suka sekali berpergian. Kali ini, mereka merencanakan untuk berpergian ke kota Zimby. Karena kesibukan mereka masing-masing, mereka baru bisa berangkat saat dini hari. Rencana berpergian pun dibuat serapi dan sedetail mungkin agar petualangan kali ini dapat memberikan kenangan yang tak terlupakan.

Hari keberangkatan pun akhirnya tiba. DEANND berkumpul terlebih dahulu di kos Dodot. Perjalanan ke kota Zimby membutuhkan waktu sekitar 50 jam. Sesampainya di kota Zimby, DEANND langsung mengeluh. Hal ini dikarenakan kota Zimby sangatlah macet sehingga petualangan DEANND menjadi terhambat. Untuk mengatasi masalah tersebut, Dono yang cerdas memiliki ide untuk mengembangkan sistem cerdas bernama Smart Traffic Controller (STC).

Sistem ini terdiri dari:
- Traffic Control Center (server) sebagai pusat pengendali
- Traffic Sensor Units (client/player) yang ditempatkan di berbagai persimpangan

Setiap sensor akan mengirimkan data kondisi lalu lintas secara real-time, dan pusat akan memberikan instruksi pengaturan lampu lalu lintas. Untuk meningkatkan akurasi, sistem kini memantau beberapa titik strategis kota secara bersamaan menggunakan sensor yang tersebar di berbagai titik lokasi kota Zimby. Disini, kamu ditugaskan untuk membantu Dono membangun sistem komunikasi ini menggunakan interprocess communication (IPC).

## Deskripsi Sistem (System Description)
Pada sistem ini akan terdapat 1 Traffic Control Center (server) dan 2 Traffic Sensor Units (client).

### Lokasi Monitoring
Semua sensor akan mengirim data untuk lokasi yang sama. Terdapat 4 titik lokasi yang harus dipantau sebagai berikut:
| Kode | Lokasi  |
| ---- | ------- |
| A    | Barat   |
| B    | Timur   |
| C    | Selatan |
| D    | Utara   |

### Traffic Status
Terdapat 2 status traffic sebagai berikut:
| Kode | Arti         |
| ---- | ------------ |
| L    | Low traffic  |
| H    | High traffic |

### Alokasi Sensor
Setiap sensor bertugas mengirimkan kondisi lalu lintas dari dua lokasi berbeda:
| Sensor   | Lokasi yang Dipantau      |
| -------- | ------------------------- |
| Sensor 1 | A (Barat) dan B (Timur)   |
| Sensor 2 | C (Selatan) dan D (Utara) |

### Alur Sistem
1. Setiap sensor membaca kondisi lalu lintas pada lokasi yang menjadi tanggung jawabnya
2. Sensor mengirim data ke server
3. Server memproses data dari semua sensor
4. Server mengirim hasil kondisi lalu lintas ke semua sensor
5. Proses berulang secara terus-menerus (continuous monitoring)
6. Sistem berhenti ketika salah satu sensor mengirimkan perintah exit

## Soal (Tasks)
### a. Setup Koneksi (Connection Setup)

Pada tahap ini, Anda diminta untuk membuat dua buah program, yaitu `server.c` yang berperan sebagai pusat kontrol sistem, serta `sensor.c` yang berperan sebagai client sensor. Komunikasi antara server dan sensor harus menggunakan mekanisme `Message Queue`. Dalam implementasinya, sistem harus menggunakan dua buah message queue yang berfungsi untuk komunikasi dua arah, yaitu:
- Satu queue digunakan untuk mengirim data dari sensor ke server.
- Satu queue lainnya digunakan untuk mengirim respon dari server ke sensor.

Program sensor dijalankan tanpa argumen tambahan. Sensor yang pertama kali dijalankan akan secara otomatis ditetapkan sebagai Sensor 1, sedangkan sensor yang dijalankan berikutnya akan menjadi Sensor 2. Server harus mampu mendeteksi koneksi dari kedua sensor tersebut dan hanya melanjutkan proses ketika kedua sensor telah terhubung.

#### Contoh Output:
```
[SERVER] Waiting for sensors...
[SERVER] Sensor 1 connected
[SERVER] Sensor 2 connected
[SERVER] System ready!
```

### b. Input Data Sensor (Input Sensor Data)
Setelah kedua sensor berhasil terhubung ke server, masing-masing sensor diminta untuk mengirimkan kondisi traffic dari dua lokasi yang menjadi tanggung jawabnya. Sensor akan berjalan secara berulang (continuous monitoring), di mana pengguna dapat terus memasukkan data kondisi lalu lintas hingga memasukkan perintah untuk berhenti. Setiap data yang dikirim harus dituliskan dalam satu baris dengan format sebagai berikut:

```
<ID_SENSOR> <LOKASI> <STATUS>
```
Contoh:
```
1 A H
1 B L
```

Setiap baris input merepresentasikan satu laporan kondisi lalu lintas dari suatu sensor pada satu lokasi tertentu. Setiap sensor harus mengirimkan tepat dua data per siklus, di mana satu siklus dimulai saat sensor mulai input, dan berakhir setelah dua data valid dikirim ke server. Nilai ID sensor harus sesuai dengan sensor yang sedang berjalan. Lokasi yang dimasukkan harus valid (A, B, C, atau D). Status hanya boleh berupa L atau H. Jika terdapat input yang tidak valid, maka program harus meminta pengguna untuk mengulangi input tersebut. Setelah kedua input dimasukkan, sensor akan mengirimkan seluruh data tersebut ke server. Sistem ini berjalan secara terus-menerus hingga pengguna memasukkan perintah `exit`.

Jika salah satu sensor mengirimkan perintah `exit`, maka sensor tersebut mengirimkan sinyal exit ke server dan berhenti. Server akan menghentikan loop utama setelah menerima sinyal exit. Server tidak perlu menunggu siklus selesai jika data belum lengkap. Server mengirimkan sinyal shutdown ke seluruh sensor lain. Sensor lain yang masih berjalan harus menghentikan prosesnya setelah menerima sinyal tersebut. Berikut adalah tampilan saat `exit`:

Server:

```
[SERVER] Exit signal received
[SERVER] Shutting down system...
[SERVER] Cleaning up message queue...
[SERVER] Done.
```

Sensor:
```
[SENSOR] Sending exit signal...
[SENSOR] Shutting down...
```

Sensor lainnya:
```
[INFO] Another sensor has exited
[INFO] Cancelling current input...
[INFO] System shutting down...
```

### c. Pemrosesan Data pada Server
Server akan berjalan dalam sebuah loop dan terus menerima data dari kedua sensor. Pada setiap siklus, server akan menunggu hingga menerima tepat 4 data (masing-masing 2 dari setiap sensor). Server akan menunggu (blocking) hingga seluruh data diterima sebelum melakukan pemrosesan. Setelah kedua sensor mengirimkan seluruh data yang diperlukan, server akan menerima 4 informasi kondisi lalu lintas yang berasal dari masing-masing lokasi, yaitu A, B, C, dan D. Server kemudian menampilkan seluruh data yang telah diterima agar dapat diketahui kondisi tiap arah secara jelas. Berikut adalah contoh tampilan pada server:

```
[SERVER] Received data:
  A: H
  B: L
  C: L
  D: H
```

Setelah itu, server juga akan melakukan proses perhitungan dengan menghitung jumlah lokasi yang memiliki kondisi lalu lintas padat (High traffic) dan lancar (Low traffic). Hasil perhitungan tersebut kemudian dapat ditampilkan sebagai berikut:

```
[SERVER] Summary:
  High Traffic: 2
  Low Traffic : 2
```

#### Penanganan Kondisi Khusus (Exit)
Server harus dapat menangani kondisi jika salah satu sensor mengirimkan sinyal `exit` sebelum seluruh data dalam satu siklus lengkap diterima. Dalam kondisi tersebut, server tidak perlu menunggu sisa data dari sensor lain. Server dapat langsung menghentikan loop utama. Server memulai proses shutdown dan mengirimkan sinyal penghentian ke seluruh sensor.

### d. Penentuan Status
Berdasarkan jumlah lokasi yang memiliki kondisi lalu lintas padat (H), server akan menentukan status lalu lintas kota secara keseluruhan. Penentuan status dilakukan dengan aturan sebagai berikut:
| Jumlah H | Status      |
| -------- | ----------- |
| ≥ 3      | MACET TOTAL |
| 2        | PADAT       |
| ≤ 1      | LANCAR      |

Hasil penentuan status ini kemudian dikirimkan kembali oleh server kepada seluruh sensor sebagai informasi kondisi lalu lintas.

### e. Pengelolaan Thread dan Sinkronisasi
Pada program `sensor.c`, komunikasi dengan server harus dilakukan secara bersamaan (concurrent). Oleh karena itu, program ini wajib menggunakan mekanisme thread dan mutex. Program sensor terdiri dari dua bagian utama yang berjalan secara paralel:
- Thread utama bertugas untuk menerima input dari pengguna serta mengirimkan data kondisi lalu lintas ke server
- Thread tambahan bertugas untuk mendengarkan pesan yang dikirimkan oleh server secara terus-menerus, seperti hasil status kota

Karena kedua thread dapat mengakses bagian output secara bersamaan, diperlukan penggunaan mutex untuk mencegah terjadinya race condition agar tampilan output tetap rapi dan tidak saling bertabrakan. Sebagai contoh, ketika sensor menerima hasil dari server, program dapat menampilkan:
```
[INFO] Status kota saat ini: PADAT
```

Ketika sensor menerima sinyal shutdown dari server, thread pendengar harus menghentikan loop penerimaan pesan, kemudian seluruh thread pada sensor harus dihentikan, dan program keluar dengan bersih.

### f. Pembersihan Resource
Ketika sistem dihentikan (melalui perintah `exit`), server wajib melakukan pembersihan resource, khususnya message queue. Pembersihan ini dilakukan untuk menghindari penumpukan resource di sistem. Dengan demikian, sistem akan kembali dalam kondisi bersih setelah program selesai dijalankan.

### Contoh Alur Berjalannya Program
Server
```
[SERVER] Waiting for sensors...
[SERVER] Sensor 1 connected
[SERVER] Sensor 2 connected
[SERVER] System ready!

[SERVER] Received data:
  A: H
  B: L
  C: L
  D: H

[SERVER] Summary:
  High Traffic: 2
  Low Traffic : 2

[SERVER] City Status: PADAT

[SERVER] Exit signal received
[SERVER] Shutting down system...
[SERVER] Cleaning up message queue...
[SERVER] Done.
```

Sensor 1:
```
Masukkan data:

1 A H
1 B L

[INFO] Current city status: PADAT

Masukkan data:
exit
[SENSOR] Sending exit signal...
[SENSOR] Shutting down...
```

Sensor 2:
```
Masukkan data:

2 C L
2 D H

[INFO] Current city status: PADAT

Masukkan data:
[INFO] Another sensor has exited
[INFO] Cancelling current input...
[INFO] System shutting down...
```

### Cara Menjalankan
Compile:

```
gcc server.c -o server
gcc sensor.c -o sensor -lpthread
```

Run:
```
./server
./sensor (untuk tiap sensor)
```
