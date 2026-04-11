# Task 1 : Tim Biru

Erlangga sedang mengikuti kompetisi *Capture The Flag* dan bertugas sebagai blue team. Ia baru saja mengunduh sebuah file arsip bernama `evidence.zip` dari server yang di duga telah diretas. File tersebut berisi ratusan log aktivitas jaringan dan dump lalu lintas data.

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

Untuk mempermudah analisis, file-file log tersebut harus di kelompokkan ke dalam folder berdasarkan jenis serangannya. Karena waktu sangat mendesak, proses pengelompokkan ini tidak boleh di lakukan satu per satu secara manual. Buat program `classify.c`, gunakan multiprocess, buat satu child process untuk setiap jenis serangan unik yang ditemukan, dan parent menunggu semua child selesai.

Contoh output :

```
logs_dump/
├── DDoS
├── Phishing
└── Malware
```

## c. The Watcher

Erlangga curiga bahwa penyerang menanamkan backdoor pada sistem, dan mengirimkan file berbahaya berekstensi `.exe dan .pcap` ke dalam direktori bernama `honeypot/`. Mengawasi folder tersebut secara manual sangatlah melelahkan, apalagi jika file-file tersebut terus bertambah.

Oleh karena itu, bantulah Erlangga dengan membuat Daemon bernama `watcher.c` dengan ketentuan : 

- Buat folder `honeypot/` dan `quarantine/` jika belum ada.  
- Daemon harus cek folder `honeypot/` secara berkala setiap 1 detik.  
- Jika menemukan file dengan ekstensi diatas, Daemon harus memindahkan file tersebut ke folder `quarantine/.`  
- Daemon menyimpan PID nya diri sendiri ke file bernama `watcher.pid` saat di jalankan.  
- Setiap kali Daemon menemukan dan memindahkan file, ia harus mencatat aktivitas tersebut ke dalam file `security.log` (file ini di luar folder honeypot & quarantine).

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

Setelah klasifikasi log selesai, Erlangga perlu mengamankan direktori `logs_dump` ke dalam arsip dan membuat laporan rekapitulasi agar mudah di analisis oleh Tim *Digital Forensic*.  
Buatlah program (report.c) yang melakukan tugas berikut : 

- Program membaca direktori `logs_dump` dan menghitung jumlah file log yang ada di dalam masing-masing folder kategori serangan.  
- Hasil perhitungan tersebut ditulis ke dalam sebuah file bernama `threat_report.txt` dengan format seperti dibawah dan di urutkan paling besar:

```
Laporan Klasifikasi Serangan:  
<index>. <Jenis Serangan> muncul sebanyak X kali.

Top 5 List IP mencurigakan :   
<index>. <IP Address> muncul sebanyak X kali.  
```

Contoh `threat_report.txt`  
```
Laporan Klasifikasi Serangan:  
1. DDoS muncul sebanyak 10 kali.  
2. Malware muncul sebanyak 5 kali.  
3. Phising muncul sebanyak 3 kali.

Top 5 IP Mencurigakan:  
1. 97.101.10.34 muncul sebanyak 15 kali.  
2. 172.16.0.2 muncul sebanyak 10 kali.  
...  
```

- Setelah file laporan selesai ditulis, program membuat child process untuk mengarsipkan seluruh direktori `logs_dump` beserta isinya dan file `threat_report.txt` menjadi satu file bernama `backup_evidence.zip`   
- Parent process menunggu proses pengarsipan selesai lalu mencetak pesan :  
  `[Info] Laporan dan arsip berhasil dibuat`


Note : Tidak boleh menggunakan `system()` atau `popen()`. Disarankan menggunakan `fork/exec`.  