# Task 2 (_DEANND'S Adventure_)

Dustin, Eli, Anindya, Nicho, Nixon, Darryl (DEANND) adalah segerombolan mahasiswa yang suka sekali berpergian. Kali ini, mereka merencanakan untuk berpergian ke kota Zimby. Karena kesibukan mereka masing-masing, mereka baru bisa berangkat saat dini hari. Rencana berpergian pun dibuat serapi dan sedetail mungkin agar petualangan kali ini dapat memberikan kenangan yang tak terlupakan.

Hari keberangkatan pun akhirnya tiba. DEANND berkumpul terlebih dahulu di kos Darryl. Perjalanan ke kota Zimby membutuhkan waktu sekitar 50 jam. Sesampainya di kota Zimby, DEANND langsung mengeluh. Hal ini dikarenakan kota Zimby sangatlah macet sehingga petualangan DEANND menjadi terhambat. Untuk mengatasi masalah tersebut, Dustin yang cerdas memiliki ide untuk mengembangkan sistem cerdas bernama Smart Traffic Controller (STC).

Sistem ini terdiri dari:
- Traffic Control Center (server) sebagai pusat pengendali
- Traffic Sensor Units (client/player) yang ditempatkan di berbagai persimpangan

Setiap sensor akan mengirimkan data kondisi lalu lintas secara real-time, dan pusat akan memberikan instruksi pengaturan lampu lalu lintas. Untuk meningkatkan akurasi, sistem kini memantau beberapa titik strategis kota secara bersamaan menggunakan sensor yang tersebar di berbagai titik lokasi kota Zimby. Disini, kamu ditugaskan untuk membantu Darryl membangun sistem komunikasi ini menggunakan interprocess communication (IPC).

## Deskripsi Sistem
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

## Soal (_Tasks_)
### a. Setup Koneksi (_Connection Setup_)

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

### b. Input Data Sensor (_Input Sensor Data_)
Setelah kedua sensor berhasil terhubung ke server, masing-masing sensor diminta untuk mengirimkan kondisi traffic dari dua lokasi yang menjadi tanggung jawabnya. Sensor akan berjalan secara berulang (continuous monitoring), di mana pengguna dapat terus memasukkan data kondisi lalu lintas hingga memasukkan perintah untuk berhenti. Setiap data yang dikirim harus dituliskan dalam satu baris dengan format sebagai berikut:

```
<ID_SENSOR> <LOKASI> <STATUS>
```
Contoh:
```
1 A H
1 B L
```

Setiap baris input merepresentasikan satu laporan kondisi lalu lintas dari suatu sensor pada satu lokasi tertentu. Setiap sensor harus mengirimkan tepat dua data per siklus, di mana satu siklus dimulai saat sensor mulai input, dan berakhir setelah dua data valid dikirim ke server. Nilai ID sensor harus sesuai dengan sensor yang sedang berjalan. Lokasi yang dimasukkan harus valid (A, B, C, atau D) dan sesuai dengan alokasi ID. Status hanya boleh berupa L atau H. Jika terdapat input yang tidak valid, maka program harus meminta pengguna untuk mengulangi input tersebut. Setelah kedua input dimasukkan, sensor akan mengirimkan seluruh data tersebut ke server. Sistem ini berjalan secara terus-menerus hingga pengguna memasukkan perintah `exit`.

Jika salah satu sensor mengirimkan perintah `exit`, maka sensor tersebut mengirimkan sinyal exit ke server dan berhenti. Server akan menghentikan loop utama setelah menerima sinyal exit. Server tidak perlu menunggu siklus selesai jika data belum lengkap. Server mengirimkan sinyal shutdown ke seluruh sensor lain. Sensor lain yang masih berjalan harus segera menghentikan proses setelah menerima sinyal tersebut. Berikut adalah tampilan saat `exit`:

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
[INFO] Current city status: PADAT
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

### Notes
1. Program wajib menggunakan mekanisme IPC Message Queue.
2. Tidak diperbolehkan menggunakan `system()`.
3. Program sensor wajib menggunakan thread dan mutex.
5. Program harus menghindari deadlock serta race condition.
6. Server wajib melakukan cleanup message queue sebelum keluar.

# Task 2 (_DEANND'S Adventure_)
Dustin, Eli, Anindya, Nicho, Nixon, and Darryl (DEANND) are a group of college students who love to travel. This time, they planned a trip to the city of Zimby. Due to their busy schedules, they couldn't leave until early in the morning. They carefully planned their itinerary to ensure that this adventure would be unforgettable.

The day of departure finally arrived. DEANND gathered first at Darryl's boarding house. The trip to Zimby would take approximately 50 hours. Upon arriving in Zimby, DEANND immediately complained. This was because Zimby was extremely congested, hampering their adventure. To solve this problem, the brilliant Dustin came up with the idea of ​​developing an intelligent system called the Smart Traffic Controller (STC).

This system consists of:
- Traffic Control Center (server) as the control center
- Traffic Sensor Units (clients/players) placed at various intersections

Each sensor will transmit real-time traffic condition data, and the center will provide instructions for setting traffic lights. To improve accuracy, the system now monitors several strategic points in the city simultaneously using sensors spread across various locations in Zimby. Here, you are tasked with helping Darryl build this communication system using interprocess communication (IPC).

## System Description
In this system there will be 1 Traffic Control Center (server) and 2 Traffic Sensor Units (client).

### Monitoring Location
All sensors will send data for the same location. There are four locations to monitor as follows:
| Code | Location |
| ---- | ------- |
| A | West |
| B | East |
| C | South |
| D | North |

### Traffic Status
There are 2 traffic statuses as follows:
| Code | Meaning |
| ---- | ------------ |
| L | Low traffic |
| H | High traffic |

### Sensor Allocation
Each sensor transmits traffic conditions from two different locations:
| Sensor | Monitored Location |
| -------- | ------------------------- |
| Sensor 1 | A (West) and B (East) |
| Sensor 2 | C (South) and D (North) |

### System Flow
1. Each sensor reads traffic conditions at its responsible location.
2. The sensor sends data to the server.
3. The server processes data from all sensors.
4. The server sends traffic condition results to all sensors.
5. The process repeats continuously (continuous monitoring).
6. The system stops when one of the sensors sends an exit command.

## Tasks
### a. Connection Setup

In this step, you are asked to create two programs: `server.c`, which acts as the system control center, and `sensor.c`, which acts as the sensor client. Communication between the server and the sensor must use the `Message Queue` mechanism. In its implementation, the system must use two message queues for two-way communication:
- One queue is used to send data from the sensor to the server.
- The other queue is used to send responses from the server to the sensor.

The sensor program is executed without any additional arguments. The first sensor executed will be automatically designated as Sensor 1, while the next sensor executed will be Sensor 2. The server must be able to detect the connection between the two sensors and only continue processing once both sensors are connected.

#### Example Output:
```
[SERVER] Waiting for sensors...
[SERVER] Sensor 1 connected
[SERVER] Sensor 2 connected
[SERVER] System ready!
```
### b. Input Sensor Data
Once both sensors are successfully connected to the server, each sensor is asked to transmit traffic conditions from its two responsible locations. The sensors will run continuously, allowing the user to continuously input traffic condition data until a stop command is issued. Each transmitted data item must be written on a single line with the following format:

```
<ID_SENSOR> <LOKASI> <STATUS>
```
Example:
```
1 A H
1 B L
```

Each input line represents a single traffic condition report from a sensor at a specific location. Each sensor must send exactly two data points per cycle, with a cycle beginning when the sensor begins input and ending after two valid data points have been sent to the server. The sensor ID value must match the currently running sensor. The entered location must be valid (A, B, C, or D) and adjust to ID allocation. The status can only be L or H. If an invalid input is entered, the program must prompt the user to re-enter it. After both input points are entered, the sensor sends all data to the server. The system runs continuously until the user enters the `exit` command.

If any sensor sends an `exit` command, it sends an exit signal to the server and stops. The server stops the main loop upon receiving the exit signal. The server does not need to wait for the cycle to complete if the data is incomplete. The server sends a shutdown signal to all other sensors. Any remaining sensors must stop processing immediately upon receiving the signal. Here's what the `exit` screen looks like:

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

Another sensor:
```
[INFO] Another sensor has exited
[INFO] Cancelling current input...
[INFO] System shutting down...
```

### c. Data Processing on the Server
The server will run in a loop, continuously receiving data from both sensors. In each cycle, the server will wait until it receives exactly four data points (two from each sensor). The server will wait (block) until all data is received before processing. After both sensors have transmitted all the necessary data, the server will receive four pieces of traffic condition information from each location: A, B, C, and D. The server then displays all the received data so that conditions in each direction can be clearly understood. The following is an example of the server display:

```
[SERVER] Received data:
  A: H
  B: L
  C: L
  D: H
```

Afterward, the server will perform a calculation process by counting the number of locations with heavy (high traffic) and smooth (low traffic) traffic. The results of this calculation can then be displayed as follows:

```
[SERVER] Summary:
  High Traffic: 2
  Low Traffic : 2
```

#### Handling Conditions (Exit)
The server must be able to handle the situation where one of the sensors sends an exit signal before all data for a complete cycle has been received. In this situation, the server does not need to wait for the remaining data from the other sensors. The server can immediately stop the main loop. The server initiates a shutdown process and sends a stop signal to all sensors.

### d. Status Determination
Based on the number of locations experiencing heavy traffic (H), the server will determine the overall city traffic status. Status determination is carried out using the following rules:
| Number o H | Status |
| -------- | ----------- |
| ≥ 3 | MACET TOTAL |
| 2 | PADAT |
| ≤ 1 | LANCAR |

The results of this status determination are then sent back by the server to all sensors as traffic condition information.

### e. Thread Management and Synchronization
In the `sensor.c` program, communication with the server must be concurrent. Therefore, this program must use thread and mutex mechanisms. The sensor program consists of two main parts that run in parallel:
- The main thread is responsible for receiving input from the user and sending traffic condition data to the server.
- An additional thread is responsible for continuously listening for messages sent by the server, such as city status results.

Because both threads can access the output simultaneously, mutexes are required to prevent race conditions and ensure the output display remains clean and does not collide with each other. For example, when the sensor receives results from the server, the program might display:
```
[INFO] Current city status: PADAT
```

When the sensor receives a shutdown signal from the server, the listening thread must stop the message receiving loop, then all threads on the sensor must be terminated, and the program exits cleanly.

### f. Resource Cleanup
When the system is terminated (via the `exit` command), the server is required to clean up resources, particularly the message queue. This cleanup is performed to prevent resource buildup in the system. This ensures the system returns to a clean state after the program completes its execution.

### Program Example
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

### How to Run
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

### Notes
1. Programs musr use the IPC Message Queue mechanism.
2. Using `system()` is prohibited.
3. Sensor programs must use threads and mutexes.
5. Programs must avoid deadlocks and race conditions.
6. The server must clean up the message queue before exiting.




