# Task 2 (Otomasi Pabrik Talos-II)

Sebagai teknisi senior di bawah arahan Perlica, kamu ditugaskan untuk mengoptimalkan Aggregation Node (Basis Utama) milik Endfield Industries di Talos-II. Sistem ini harus mengumpulkan sumber daya mentah dan merakitnya menjadi komponen berharga secara bersamaan.

Karena keterbatasan memori sistem lokal, kamu diminta menjalankan seluruh proses ini dalam satu program C bernama endfield.c, membaginya ke dalam berbagai threads, menggunakan pipe untuk pelaporan, serta menyediakan interactive terminal interface untuk mengatur laju produksi.

Bantulah Endfield Industries mengotomatisasi fasilitas ini agar berjalan efisien dan aman dari Race Condition!

## Deskripsi Pabrik *(Factory Description)*

Sistem pabrik ini bergantung pada variabel global (*inventory*) yang menyimpan sumber daya mentah dan hasil produksi. 

### Sumber Daya Mentah *(Raw Resources)*

Terdapat 4 jenis sumber daya yang ditambang secara otomatis:
* **Originium**
* **Amethyst**
* **Ferrium**
* **Buckflower** *(Hint: Buckflower memiliki resep dengan nilai desimal)*

### Resep Produksi *(Production Recipes)*

Terdapat 5 jenis komponen yang dapat dirakit menggunakan sumber daya di atas. Perakitan 1 unit komponen memakan waktu **10 detik**:
1. **HC Valley Battery** : 20 Ferrium + 30 Originium
2. **SC Valley Battery** : 10 Ferrium + 15 Originium
3. **Buck Capsule (A)** : 40 Ferrium + 10 Buckflower
4. **Buck Capsule (C)** : 10 Amethyst + 2.5 Buckflower
5. **Cryston Component** : 20 Originium + 20 Amethyst

## Deskripsi Tugas (Tasks)

### a. Resourcing Threads

Sistem memiliki fasilitas penambangan otomatis. Buatlah 4 thread yang masing-masing bertugas mengumpulkan sumber daya mentah: Originium, Amethyst, Ferrium, dan Buckflower.

- Semua hasil tambang disimpan dalam variabel global yang bertindak sebagai inventory.
- Masing-masing thread secara terus-menerus menambahkan sumber daya ke inventory dengan interval waktu 1 detik (sleep(1)).
- Jumlah yield (hasil tambang) yang ditambahkan per detiknya bersifat dinamis dan dapat diubah melalui menu terminal.

### b. Production Threads

Buatlah sistem lini produksi yang diwakili oleh thread. Jumlah thread yang berjalan untuk masing-masing formula bergantung pada production plan yang diatur melalui terminal (secara default saat program pertama dijalankan, seluruh lini produksi bernilai 0).

Setiap thread produksi akan mengecek inventory secara terus-menerus. Jika bahan baku mencukupi, thread akan memotong jumlah bahan baku tersebut dari inventory dan memproduksi 1 unit item. Proses perakitan memakan waktu 10 detik (sleep(10)) per item.

Berikut adalah resep formulasinya:

- HC Valley Battery : 20 Ferrium + 30 Originium
- SC Valley Battery : 10 Ferrium + 15 Originium
- Buck Capsule (A) : 40 Ferrium + 10 Buckflower
- Buck Capsule (C) : 10 Amethyst + 2.5 Buckflower
- Cryston Component : 20 Originium + 20 Amethyst

### c. The Pipeline Monitor

Pemantauan sistem berjalan di background dan direkam ke dalam log file, sehingga tidak mengganggu main terminal interface. Gunakan teknik Multiprocessing dan Piping!

- Di awal eksekusi program endfield.c, lakukan pipe() lalu fork().
- Parent Process akan bertugas menjalankan main menu interface beserta seluruh thread di atas. Selain itu, Parent Process membuat 1 thread tambahan yaitu Reporter Thread.
- Setiap 5 detik, Reporter Thread membaca status inventory dan total produksi, menyusunnya menjadi sebuah teks (string), lalu mengirimkannya melalui Pipe.
- Child Process bertindak sebagai Monitor. Ia membaca data dari Pipe secara terus-menerus dan menyimpannya ke dalam file log_produksi.txt (Hint: pastikan file log selalu dalam keadaan bersih/kosong di awal program berjalan). Child process tidak boleh mencetak apapun ke terminal agar tidak mengganggu interaksi user.

Contoh format teks yang dikirimkan ke Pipe dan disimpan di log:

```
[LIVE DASHBOARD] - Status Pabrik Talos-II
Inventory: Originium: 35 | Amethyst: 12 | Ferrium: 42 | Buckflower: 10
Hasil Produksi: HC Battery: 0 | SC Battery: 1 | Buck(A): 0 | Buck(C): 0 | Cryston: 0
```
----------------------------------------------------------------------

### d. Manual Override (Instant Crafting)

Terkadang Endministrator membutuhkan komponen secara mendesak tanpa harus menunggu lini produksi otomatis. Tambahkan fitur Manual Crafting pada menu terminal!

Saat pengguna memilih menu ini, program akan:

1. Meminta pengguna memilih komponen yang ingin dibuat (menggunakan resep yang sama dengan lini produksi otomatis).
2. Meminta pengguna memasukkan jumlah komponen yang ingin dibuat sekaligus.
3. Program akan mengecek ketersediaan bahan baku di inventory.
4. Jika cukup, sistem akan langsung memotong bahan baku dan menambahkan hasil produksi tanpa delay (sleep), lalu mencetak pesan berhasil.
5. Jika tidak cukup, cetak pesan bahwa bahan baku kurang dan batalkan proses.

----------------------------------------------------------------------

### e. Valley-IV Terminal & Emergency Halt

Main thread dari Parent Process bertugas untuk menampilkan interactive interface yang terus berjalan menggunakan looping. Karena input (scanf) berada di main thread, ia tidak akan melakukan blocking terhadap thread produksi dan resource yang berjalan di background.

Menu yang harus ditampilkan:
```
--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
```

Detail Menu:

- Menu 1: Mengubah yield/detik dari setiap resource (Originium, Amethyst, Ferrium, Buckflower).
- Menu 2: Menentukan berapa jumlah lini produksi (threads) yang aktif untuk masing-masing formula (misal: mengatur lini HC Battery menjadi 2, berarti ada 2 thread HC Battery yang berjalan bersamaan).
- Menu 3: Melakukan Manual Override (Instant Crafting) sesuai dengan deskripsi poin d.
- Menu 4 (Exit): Pabrik harus bisa dihentikan dengan aman. Memilih Menu 4 akan memicu protokol graceful shutdown.

Protokol Graceful Shutdown:

Saat mematikan sistem, program harus menghentikan semua thread penambangan dan produksi, mengirimkan pesan terakhir ke Pipe ("Sistem dihentikan. Memulai prosedur shutdown..."), menutup file descriptor Pipe, menghancurkan Mutex/sinkronisasi lainnya, lalu keluar dari program dengan aman. Program tidak boleh mati secara paksa (crash).

## Contoh Jalannya Program (Example Run)

> Compile program dengan: gcc endfield.c -o endfield -lpthread  

```text
[SYSTEM] Starting Endfield Automation System...
[SYSTEM] Background threads initiated. Monitor Active.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 1

-- Modify Resource Gains --
1. Originium (current yield: 1/sec)
2. Amethyst (current yield: 1/sec)
3. Ferrium (current yield: 1/sec)
4. Buckflower (current yield: 1/sec)
Select resource: 3
New yield for Ferrium: 5
[SUCCESS] Ferrium yield updated to 5/sec.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 2

-- Change Production Plan --
1. HC Valley Battery (Active lines: 0)
2. SC Valley Battery (Active lines: 0)
3. Buck Capsule (A)  (Active lines: 0)
4. Buck Capsule (C)  (Active lines: 0)
5. Cryston Component (Active lines: 0)
Select formula: 2
Set number of active threads for SC Valley Battery: 1
[SUCCESS] SC Valley Battery production line updated to 1 thread(s).

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 3

-- Manual Override (Instant Crafting) --
1. HC Valley Battery (20 Ferrium, 30 Originium)
2. SC Valley Battery (10 Ferrium, 15 Originium)
3. Buck Capsule (A)  (40 Ferrium, 10 Buckflower)
4. Buck Capsule (C)  (10 Amethyst, 2.5 Buckflower)
5. Cryston Component (20 Originium, 20 Amethyst)
Select formula to craft: 1
Amount to craft: 1
[SYSTEM] Checking inventory...
[SUCCESS] Instant craft complete! 1x HC Valley Battery added to inventory.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 4

[SYSTEM] System halted. Initiating shutdown procedure...
[SYSTEM] All machines safely powered down. Goodbye.
```

(Catatan: Sementara interaksi di atas terjadi, file log_produksi.txt akan terus terisi setiap 5 detik oleh child process di background).

---

# Task 2 (Talos-II Automated Factory)

As a senior technician under Perlica's direction, you are tasked with optimizing Endfield Industries' Aggregation Node on Talos-II. The system must gather raw resources and assemble them into valuable components concurrently.

Due to local system memory constraints, you are required to run this entire process within a single C program named endfield.c, dividing the workload into multiple threads, using a pipe for reporting, and providing an interactive terminal interface to manage production rates.

Help Endfield Industries automate this facility so it runs efficiently and safely from Race Conditions!

## Factory Description

This factory system relies on global variables (*inventory*) to store raw resources and produced items.

### Raw Resources

There are 4 types of resources that are mined automatically:
* **Originium**
* **Amethyst**
* **Ferrium**
* **Buckflower** *(Hint: there exists a Buckflower formula which uses fractions/floating number).*

### Production Recipes

There are 5 types of components that can be assembled using the resources above. Assembling 1 unit of any component takes **10 seconds**:
1. **HC Valley Battery** : 20 Ferrium + 30 Originium
2. **SC Valley Battery** : 10 Ferrium + 15 Originium
3. **Buck Capsule (A)** : 40 Ferrium + 10 Buckflower
4. **Buck Capsule (C)** : 10 Amethyst + 2.5 Buckflower
5. **Cryston Component** : 20 Originium + 20 Amethyst

## Tasks

### a. Resourcing Threads

The system has automated mining facilities. Create 4 threads, each responsible for gathering a specific raw resource: Originium, Amethyst, Ferrium, and Buckflower.

- All mined resources are stored in global variables acting as the inventory.
- Each thread continuously adds resources to the inventory at a 1-second interval (sleep(1)).
- The yield amount added per second is dynamic and can be modified via the terminal menu.

### b. Production Threads

Create a production line system represented by threads. The number of running threads for each formula depends on the production plan configured via the terminal (by default, when the program is first executed, all production lines are set to 0).

Each production thread will continuously check the inventory. If the required raw materials are sufficient, the thread will deduct those materials from the inventory and produce 1 unit of the item. The assembly process takes 10 seconds (sleep(10)) per item.

Here are the formulation recipes:

- HC Valley Battery : 20 Ferrium + 30 Originium
- SC Valley Battery : 10 Ferrium + 15 Originium
- Buck Capsule (A) : 40 Ferrium + 10 Buckflower
- Buck Capsule (C) : 10 Amethyst + 2.5 Buckflower
- Cryston Component : 20 Originium + 20 Amethyst

### c. The Pipeline Monitor

System monitoring runs in the background and is recorded into a log file, ensuring it does not interfere with the main terminal interface. Utilize Multiprocessing and Piping!

- At the beginning of endfield.c execution, call pipe() followed by fork().
- The Parent Process is responsible for running the main menu interface and all the threads mentioned above. Additionally, the Parent Process creates 1 extra thread: the Reporter Thread.
- Every 5 seconds, the Reporter Thread reads the inventory status and total production counts, formats it into a text string, and sends it through the Pipe.
- The Child Process acts as the Monitor. It continuously reads data from the Pipe and writes it to a file named log_produksi.txt (Hint: ensure the log file is clean/empty every time the program starts). The child process must not print anything to the terminal to avoid disrupting user interaction.

Example format of the text sent to the Pipe and saved in the log:

```
[LIVE DASHBOARD] - Talos-II Factory Status
Inventory: Originium: 35 | Amethyst: 12 | Ferrium: 42 | Buckflower: 10
Production: HC Battery: 0 | SC Battery: 1 | Buck(A): 0 | Buck(C): 0 | Cryston: 0
```
----------------------------------------------------------------------

### d. Manual Override (Instant Crafting)

Sometimes the Endministrator needs components urgently without waiting for the automated production lines. Add a Manual Crafting feature to the terminal menu!

When the user selects this menu, the program will:

1. Ask the user to choose the component to craft (using the same recipes as the automated production lines).
2. Ask the user to input the amount of components to craft at once.
3. The program will check the availability of raw materials in the inventory.
4. If sufficient, the system will instantly deduct the raw materials and add the produced items without any delay (sleep), then print a success message.
5. If insufficient, print a message stating that the raw materials are inadequate and cancel the process.

----------------------------------------------------------------------

### e. Valley-IV Terminal & Emergency Halt

The main thread of the Parent Process is responsible for displaying a continuous interactive interface using a loop. Because the input (scanf) resides in the main thread, it will not block the background resourcing and production threads.

The required menu display:
```
--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
```

Menu Details:

- Menu 1: Modify the yield/second of each resource (Originium, Amethyst, Ferrium, Buckflower).
- Menu 2: Set the number of active production lines (threads) for each formula (e.g., setting the HC Battery line to 2 means 2 HC Battery threads will run concurrently).
- Menu 3: manual crafting
- Menu 4 (Exit): The factory must be able to shut down safely. Choosing Menu 4 will trigger the graceful shutdown protocol.

Graceful Shutdown Protocol:

When shutting down, the program must halt all resourcing and production threads, send a final message to the Pipe ("System halted. Initiating shutdown procedure..."), close all Pipe file descriptors, destroy the Mutex/synchronizations, and exit the program safely without crashing.

#### Example Run

> Compile the program with: gcc endfield.c -o endfield -lpthread  

```
[SYSTEM] Starting Endfield Automation System...
[SYSTEM] Background threads initiated. Monitor Active.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 1

-- Modify Resource Gains --
1. Originium (current yield: 1/sec)
2. Amethyst (current yield: 1/sec)
3. Ferrium (current yield: 1/sec)
4. Buckflower (current yield: 1/sec)
Select resource: 3
New yield for Ferrium: 5
[SUCCESS] Ferrium yield updated to 5/sec.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 2

-- Change Production Plan --
1. HC Valley Battery (Active lines: 0)
2. SC Valley Battery (Active lines: 0)
3. Buck Capsule (A)  (Active lines: 0)
4. Buck Capsule (C)  (Active lines: 0)
5. Cryston Component (Active lines: 0)
Select formula: 2
Set number of active threads for SC Valley Battery: 1
[SUCCESS] SC Valley Battery production line updated to 1 thread(s).

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 3

-- Manual Override (Instant Crafting) --
1. HC Valley Battery (20 Ferrium, 30 Originium)
2. SC Valley Battery (10 Ferrium, 15 Originium)
3. Buck Capsule (A)  (40 Ferrium, 10 Buckflower)
4. Buck Capsule (C)  (10 Amethyst, 2.5 Buckflower)
5. Cryston Component (20 Originium, 20 Amethyst)
Select formula to craft: 1
Amount to craft: 1
[SYSTEM] Checking inventory...
[SUCCESS] Instant craft complete! 1x HC Valley Battery added to inventory.

--------- [ VALLEY-IV TERMINAL ] ---------
1. Modify resource gains
2. Change production plan
3. Manual override (Instant Crafting)
4. Exit
------------------------------------------
> 4

[SYSTEM] System halted. Initiating shutdown procedure...
[SYSTEM] All machines safely powered down. Goodbye.
```

(Note: While the above interaction occurs, the log_produksi.txt file will continuously be appended every 5 seconds by the background process).