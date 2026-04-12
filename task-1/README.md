# Task 1 : Tim Biru 

Erlangga sedang mengikuti kompetisi *Capture The Flag* dan bertugas sebagai blue team. Ia baru saja mengunduh sebuah file arsip bernama `evidence.zip` dari server yang diduga telah diretas. File tersebut berisi ratusan log aktivitas jaringan dan dump lalu lintas data.

Karena banyaknya file tersebut, Erlangga harus menganalisis bukti-bukti tersebut secepat mungkin. Ia membutuhkan sistem otomatis agar pekerjaannya lebih efisien.

Berikut adalah tugas-tugas yang harus diselesaikan untuk membantu keamanan tersebut : 


## a. Initial Extraction

Erlangga tidak ingin membuang waktu mengekstrak file secara manual. Buatlah sebuah program (`extract.c`) yang secara otomatis : 

- Mengekstrak file `evidence.zip` ke dalam sebuah direktori baru bernama `logs_dump`  
- Setelah proses ekstraksi selesai, program menghapus file `evidence.zip`

Note : Proses ekstraksi dan penghapusan harus sekuensial. Tidak boleh menggunakan `system()` atau `popen()`.

## b. Logs Classification

Di dalam direktori `logs_dump/` terdapat banyak file log dengan format penamaan 
```
[Jenis Serangan]_[IP Address]_[Timestamp (YYYYMMDD_HHMMSS)].log
```
 

Contohnya : `DDoS_192.168.1.10_20240601_120000.log`, `Phishing_10.0.0.5_20240601_130000.log`, `Malware_172.16.0.1_20240601_140000.log`.

Untuk mempermudah analisis, file-file log tersebut harus dikelompokkan ke dalam folder berdasarkan jenis serangannya. Karena waktu sangat mendesak, proses pengelompokan ini tidak boleh dilakukan satu per satu secara manual. Buat program `classify.c`, gunakan multiprocess, buat satu child process untuk setiap jenis serangan unik yang ditemukan, dan parent menunggu semua child selesai.

Contoh output :

```
logs_dump/
├── DDoS
├── Phishing
└── Malware
```

## c. The Watcher

Erlangga curiga bahwa penyerang menanamkan backdoor pada sistem, dan mengirimkan file berbahaya berekstensi `.exe` dan `.pcap` ke dalam direktori bernama `honeypot/`. Mengawasi folder tersebut secara manual sangatlah melelahkan, apalagi jika file-file tersebut terus bertambah.

Oleh karena itu, bantulah Erlangga dengan membuat Daemon bernama `watcher.c` dengan ketentuan : 

- Buat folder `honeypot/` dan `quarantine/` jika belum ada.  
- Daemon harus cek folder `honeypot/` secara berkala setiap 1 detik.  
- Jika menemukan file dengan ekstensi di atas, Daemon harus memindahkan file tersebut ke folder `quarantine/`.  
- Daemon menyimpan PID-nya sendiri ke file bernama `watcher.pid` saat dijalankan.  
- Setiap kali Daemon menemukan dan memindahkan file, ia harus mencatat aktivitas tersebut ke dalam file `security.log`.

Untuk menghentikan daemon gunakan perintah :   
```
kill $(cat watcher.pid)
```
  
Pastikan ketika daemon menerima `SIGTERM`, daemon harus menutup file `security.log` dengan benar sebelum berhenti.

Format output pada `security.log`  
```
[YYYY-MM-DD HH:MM:SS] Peringatan! Menemukan file mencurigakan: <nama_file>.<ekstensi>  
[YYYY-MM-DD HH:MM:SS] Berhasil mengkarantina file: <nama_file>.<ekstensi>  
```

Contoh output pada `security.log`  
```
[2024-06-01 12:00:00] Peringatan! Menemukan file mencurigakan: backdoor.exe  
[2024-06-01 12:00:01] Berhasil mengkarantina file: backdoor.exe  
```

Note : Tidak boleh menggunakan `inotify` atau `fanotify`  

## d. Post Incident Archiving & Reporting

Setelah klasifikasi log selesai, Erlangga perlu mengamankan direktori `logs_dump` ke dalam arsip dan membuat laporan rekapitulasi agar mudah dianalisis oleh Tim *Digital Forensic*.  
Buatlah program (report.c) yang melakukan tugas berikut : 

- Program membaca direktori `logs_dump` dan menghitung jumlah file log yang ada di dalam masing-masing folder kategori serangan.  
- Hasil perhitungan tersebut ditulis ke dalam sebuah file bernama `threat_report.txt` dengan format seperti di bawah dan diurutkan paling besar:

```
Laporan Klasifikasi Serangan:  
<index>. <Jenis Serangan> muncul sebanyak X kali.

Top 5 IP Mencurigakan:   
<index>. <IP Address> muncul sebanyak X kali.  
```

Contoh `threat_report.txt`  
```
Laporan Klasifikasi Serangan:  
1. DDoS muncul sebanyak 10 kali.  
2. Malware muncul sebanyak 5 kali.  
3. Phishing muncul sebanyak 3 kali.

Top 5 IP Mencurigakan:  
1. 97.101.10.34 muncul sebanyak 15 kali.  
2. 172.16.0.2 muncul sebanyak 10 kali.  
...  
```

- Setelah file laporan selesai ditulis, program membuat child process untuk mengarsipkan seluruh direktori `logs_dump` beserta isinya dan file `threat_report.txt` menjadi satu file bernama `backup_evidence.zip`   
- Parent process menunggu proses pengarsipan selesai lalu mencetak pesan :  
  `[Info] Laporan dan arsip berhasil dibuat`


Note : Tidak boleh menggunakan `system()` atau `popen()`. Disarankan menggunakan `fork/exec`.  


---

# Task 1: Blue Team

Erlangga is participating in a *Capture The Flag* competition and is assigned as a blue team member. He has just downloaded an archive file called `evidence.zip` from a server suspected of being hacked. The file contains hundreds of network activity logs and data traffic dumps.

Due to the large number of files, Erlangga needs to analyze the evidence as quickly as possible. He needs an automated system to make his work more efficient.

Here are the tasks that must be completed to assist with the security investigation:


## a. Initial Extraction

Erlangga doesn't want to waste time extracting files manually. Create a program (`extract.c`) that automatically:

- Extracts the `evidence.zip` file into a new directory called `logs_dump`
- After the extraction process is complete, the program deletes the `evidence.zip` file

Note: The extraction and deletion process must be sequential. The use of `system()` or `popen()` is not allowed.

## b. Logs Classification

Inside the `logs_dump/` directory, there are many log files with the naming format:
```
[Attack Type]_[IP Address]_[Timestamp (YYYYMMDD_HHMMSS)].log
```

For example: `DDoS_192.168.1.10_20240601_120000.log`, `Phishing_10.0.0.5_20240601_130000.log`, `Malware_172.16.0.1_20240601_140000.log`.

To simplify analysis, the log files must be grouped into folders based on their attack type. Since time is critical, this grouping process must not be done one by one manually. Create a program `classify.c`, use multiprocessing — create one child process for each unique attack type found, and have the parent wait for all children to finish.

Example output:

```
logs_dump/
├── DDoS
├── Phishing
└── Malware
```

## c. The Watcher

Erlangga suspects that the attacker has planted a backdoor on the system and is sending malicious files with `.exe` and `.pcap` extensions into a directory called `honeypot/`. Monitoring that folder manually is exhausting, especially when files keep coming in.

Therefore, help Erlangga by creating a Daemon called `watcher.c` with the following requirements:

- Create the `honeypot/` and `quarantine/` folders if they do not already exist.
- The Daemon must check the `honeypot/` folder periodically every 1 second.
- If it finds a file with the above extensions, the Daemon must move that file to the `quarantine/` folder.
- The Daemon saves its own PID to a file called `watcher.pid` when started.
- Every time the Daemon finds and moves a file, it must log the activity to a file called `security.log`.

To stop the daemon, use the command:
```
kill $(cat watcher.pid)
```

Ensure that when the daemon receives `SIGTERM`, it must properly close the `security.log` file before stopping.

Log output format in `security.log`:
```
[YYYY-MM-DD HH:MM:SS] Peringatan! Menemukan file mencurigakan: <nama_file>.<ekstensi>  
[YYYY-MM-DD HH:MM:SS] Berhasil mengkarantina file: <nama_file>.<ekstensi>  
```

Example output in `security.log`:
```
[2024-06-01 12:00:00] Peringatan! Menemukan file mencurigakan: backdoor.exe
[2024-06-01 12:00:01] Berhasil mengkarantina file: backdoor.exe
```

Note: The use of `inotify` or `fanotify` is not allowed.

## d. Post Incident Archiving & Reporting

After log classification is complete, Erlangga needs to secure the `logs_dump` directory into an archive and create a summary report for easy analysis by the *Digital Forensics* Team.
Create a program (`report.c`) that performs the following tasks:

- The program reads the `logs_dump` directory and counts the number of log files within each attack category folder.
- The results are written to a file called `threat_report.txt` in the following format, sorted from largest to smallest:

```
Laporan Klasifikasi Serangan:  
<index>. <Jenis Serangan> muncul sebanyak X kali.

Top 5 IP Mencurigakan:   
<index>. <IP Address> muncul sebanyak X kali.  
```

Contoh `threat_report.txt`  
```
Laporan Klasifikasi Serangan:  
1. DDoS muncul sebanyak 10 kali.  
2. Malware muncul sebanyak 5 kali.  
3. Phishing muncul sebanyak 3 kali.

Top 5 IP Mencurigakan:  
1. 97.101.10.34 muncul sebanyak 15 kali.  
2. 172.16.0.2 muncul sebanyak 10 kali.  
...  
```

- After the report file is finished writing, the program creates a child process to archive the entire `logs_dump` directory along with its contents and the `threat_report.txt` file into a single file called `backup_evidence.zip`
- The parent process waits for the archiving process to finish, then prints the message:
  `[Info] Laporan dan arsip berhasil dibuat`

Note: The use of `system()` or `popen()` is not allowed. It is recommended to use `fork/exec`.