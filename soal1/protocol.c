#include "protocol.h"
#include <time.h>
#include <stdio.h>

//fungsi waktu saat ini
void get_current_time(char *buffer) {
time_t now = time(NULL); //waktu mentah dr os
struct tm *t = localtime(&now); //waktu jd jam menit tahun

//format: YYYY-MM-DD HH:MM:SS
//4 thn, 2bln/tanggal/jam dg awalan nol jika angkanya 1-9
sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, //di c, th dihitung dari 1900&bulan dihitung dari 0
t->tm_hour, t->tm_min, t->tm_sec);
}
