#include "config.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parasail.h"
#include "blosum/blosum62.h"
#include "stats.h"
#include "timer.h"
#include "timer_real.h"

static double pctull(unsigned long long orig_, unsigned long long new_)
{
    double orig = (double)orig_;
    double new = (double)new_;
    return orig / new;
}

static double pctf(double orig, double new)
{
    return orig / new;
}

static void print_array(
        const char * filename,
        const int * const restrict array,
        const char * const restrict s1, const int s1Len,
        const char * const restrict s2, const int s2Len)
{
    int i;
    int j;
    FILE *f = fopen(filename, "w");
    fprintf(f, " ");
    for (j=0; j<s2Len; ++j) {
        fprintf(f, "%4c", s2[j]);
    }
    fprintf(f, "\n");
    for (i=0; i<s1Len; ++i) {
        fprintf(f, "%c", s1[i]);
        for (j=0; j<s2Len; ++j) {
            fprintf(f, "%4d", array[i*s2Len + j]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

typedef struct func {
    parasail_result_t* (*f)(const char * const restrict s1, const int s1Len,
                            const char * const restrict s2, const int s2Len,
                            const int open, const int gap,
                            const int matrix[24][24]);
    const char * alg;
    const char * type;
    const char * isa;
    const char * bits;
    const char * width;
    char is_table;
    char is_ref;
} func_t;


int main(int argc, char **argv)
{
    const char *seqA = "MEFYDVAVTVGMLCIIIYLLLVRQFRYWTERNVPQLNPHLLFGDVRDVNKTHHIGEKFRQLYNELKGKHPFGGIYMFTKPVALVTDLELVKNVFVKDFQYFHDRGTYYDEKHDPLSAHLFNLEGYKWKSLRNKITPTFTSGKMKMMFPTVAAAGKQFKDYLEDAIGEQEEFELKELLARYTTDVIGTCAFGIECNSMRNPNAEFRVMGKKIFGRSRSNLQLLLMNAFPSVAKLVGIKLILPEVSDFFMNAVRDTIKYRVENNVQRNDFMDILIRMRSDKETKSDDGTLTFHEIAAQAFVFFVAGFETSSSLMAFTLYELALDQDMQDKARKCVTDVLERHNGELTYEAAMEMDYLDCVLKGWVR";
    const char *seqB = "AALGVAARAGFLAAGFASSSELSSELSSEDSAAFLAAAAGVAAFAGVFTIAAFGVAATADLLAAGLHSSSELSSELSSEDSAAFFAATAGVAALAGVLAAAAAFGVAATADFFAAGLESSSELSSELSSDDSAVFFAAAAGVATFAGVLAAAATFGVAACAGFFAAGLDSSSELSSELSSEDSAAFFAAAAGVATFTGVLAAAAACAAAACVGFFAAGLDSSSELSSELSSEDSAAFFAAAAGVAALAGVLAAAAACAGFFAAGLESSSELSSE";
    //const char *seqA = "MEFYDVAVTV";
    //const char *seqB = "AALGVAARAGFLAAGFASSS";
    const int lena = strlen(seqA);
    const int lenb = strlen(seqB);
    int score;
    int matches;
    int length;
    unsigned long long timer_rdtsc;
    unsigned long long timer_rdtsc_single;
    double timer_rdtsc_ref_mean;
    double timer_nsecs;
    double timer_nsecs_single;
    double timer_nsecs_ref_mean;
    //size_t limit = 1000;
    size_t limit = 500;
    //size_t limit = 100;
    //size_t limit = 1;
    size_t i;
    size_t index;
    func_t f;
    parasail_result_t *result = NULL;
    stats_t stats_rdtsc;
    stats_t stats_nsecs;

    func_t functions[] = {
        {nw,                        "nw", "",     "",      "",    "",   0, 1},
        {nw_scan,                   "nw", "scan", "",      "",    "",   0, 0},
#if HAVE_SSE2                      
        {nw_scan_sse2_128_32,       "nw", "scan",    "sse2",  "128", "32", 0, 0},
        {nw_scan_sse2_128_16,       "nw", "scan",    "sse2",  "128", "16", 0, 0},
        {nw_scan_sse2_128_8,        "nw", "scan",    "sse2",  "128", "8",  0, 0},
        {nw_diag_sse2_128_32,       "nw", "diag",    "sse2",  "128", "32", 0, 0},
        {nw_diag_sse2_128_16,       "nw", "diag",    "sse2",  "128", "16", 0, 0},
        {nw_diag_sse2_128_8,        "nw", "diag",    "sse2",  "128", "8",  0, 0},
        {nw_striped_sse2_128_32,    "nw", "striped", "sse2",  "128", "32", 0, 0},
        {nw_striped_sse2_128_16,    "nw", "striped", "sse2",  "128", "16", 0, 0},
        {nw_striped_sse2_128_8,     "nw", "striped", "sse2",  "128", "8",  0, 0},
#endif                             
#if HAVE_SSE41
        {nw_scan_sse41_128_32,      "nw", "scan",    "sse41", "128", "32", 0, 0},
        {nw_scan_sse41_128_16,      "nw", "scan",    "sse41", "128", "16", 0, 0},
        {nw_scan_sse41_128_8,       "nw", "scan",    "sse41", "128", "8",  0, 0},
        {nw_diag_sse41_128_32,      "nw", "diag",    "sse41", "128", "32", 0, 0},
        {nw_diag_sse41_128_16,      "nw", "diag",    "sse41", "128", "16", 0, 0},
        {nw_diag_sse41_128_8,       "nw", "diag",    "sse41", "128", "8",  0, 0},
        {nw_striped_sse41_128_32,   "nw", "striped", "sse41", "128", "32", 0, 0},
        {nw_striped_sse41_128_16,   "nw", "striped", "sse41", "128", "16", 0, 0},
        {nw_striped_sse41_128_8,    "nw", "striped", "sse41", "128", "8",  0, 0},
#endif
#if HAVE_AVX2                      
        {nw_scan_avx2_256_32,       "nw", "scan",    "avx2",  "256", "32", 0, 0},
        //{nw_scan_avx2_256_16,       "nw", "scan",    "avx2",  "256", "16", 0, 0},
        //{nw_scan_avx2_256_8,        "nw", "scan",    "avx2",  "256", "8",  0, 0},
        //{nw_diag_avx2_256_32,       "nw", "diag",    "avx2",  "256", "32", 0, 0},
        //{nw_diag_avx2_256_16,       "nw", "diag",    "avx2",  "256", "16", 0, 0},
        //{nw_diag_avx2_256_8,        "nw", "diag",    "avx2",  "256", "8",  0, 0},
        //{nw_striped_avx2_256_32,    "nw", "striped", "avx2",  "256", "32", 0, 0},
        //{nw_striped_avx2_256_16,    "nw", "striped", "avx2",  "256", "16", 0, 0},
        //{nw_striped_avx2_256_8,     "nw", "striped", "avx2",  "256", "8",  0, 0},
#endif                             

        {sg,                        "sg", "",     "",      "",    "",   0, 1},
        {sg_scan,                   "sg", "scan", "",      "",    "",   0, 0},
#if HAVE_SSE2                      
        {sg_scan_sse2_128_32,       "sg", "scan",    "sse2",  "128", "32", 0, 0},
        {sg_scan_sse2_128_16,       "sg", "scan",    "sse2",  "128", "16", 0, 0},
        {sg_scan_sse2_128_8,        "sg", "scan",    "sse2",  "128", "8",  0, 0},
        {sg_diag_sse2_128_32,       "sg", "diag",    "sse2",  "128", "32", 0, 0},
        {sg_diag_sse2_128_16,       "sg", "diag",    "sse2",  "128", "16", 0, 0},
        {sg_diag_sse2_128_8,        "sg", "diag",    "sse2",  "128", "8",  0, 0},
        {sg_striped_sse2_128_32,    "sg", "striped", "sse2",  "128", "32", 0, 0},
        {sg_striped_sse2_128_16,    "sg", "striped", "sse2",  "128", "16", 0, 0},
        {sg_striped_sse2_128_8,     "sg", "striped", "sse2",  "128", "8",  0, 0},
#endif                             
#if HAVE_SSE41
        {sg_scan_sse41_128_32,      "sg", "scan",    "sse41", "128", "32", 0, 0},
        {sg_scan_sse41_128_16,      "sg", "scan",    "sse41", "128", "16", 0, 0},
        {sg_scan_sse41_128_8,       "sg", "scan",    "sse41", "128", "8",  0, 0},
        {sg_diag_sse41_128_32,      "sg", "diag",    "sse41", "128", "32", 0, 0},
        {sg_diag_sse41_128_16,      "sg", "diag",    "sse41", "128", "16", 0, 0},
        {sg_diag_sse41_128_8,       "sg", "diag",    "sse41", "128", "8",  0, 0},
        {sg_striped_sse41_128_32,   "sg", "striped", "sse41", "128", "32", 0, 0},
        {sg_striped_sse41_128_16,   "sg", "striped", "sse41", "128", "16", 0, 0},
        {sg_striped_sse41_128_8,    "sg", "striped", "sse41", "128", "8",  0, 0},
#endif

        {sw,                        "sw", "",     "",      "",    "",   0, 1},
        {sw_scan,                   "sw", "scan", "",      "",    "",   0, 0},
#if HAVE_SSE2
        {sw_scan_sse2_128_32,       "sw", "scan",    "sse2",  "128", "32", 0, 0},
        {sw_scan_sse2_128_16,       "sw", "scan",    "sse2",  "128", "16", 0, 0},
        {sw_scan_sse2_128_8,        "sw", "scan",    "sse2",  "128", "8",  0, 0},
        {sw_diag_sse2_128_32,       "sw", "diag",    "sse2",  "128", "32", 0, 0},
        {sw_diag_sse2_128_16,       "sw", "diag",    "sse2",  "128", "16", 0, 0},
        {sw_diag_sse2_128_8,        "sw", "diag",    "sse2",  "128", "8",  0, 0},
        {sw_striped_sse2_128_32,    "sw", "striped", "sse2",  "128", "32", 0, 0},
        {sw_striped_sse2_128_16,    "sw", "striped", "sse2",  "128", "16", 0, 0},
        {sw_striped_sse2_128_8,     "sw", "striped", "sse2",  "128", "8",  0, 0},
#endif                             
#if HAVE_SSE41
        {sw_scan_sse41_128_32,      "sw", "scan",    "sse41", "128", "32", 0, 0},
        {sw_scan_sse41_128_16,      "sw", "scan",    "sse41", "128", "16", 0, 0},
        {sw_scan_sse41_128_8,       "sw", "scan",    "sse41", "128", "8",  0, 0},
        {sw_diag_sse41_128_32,      "sw", "diag",    "sse41", "128", "32", 0, 0},
        {sw_diag_sse41_128_16,      "sw", "diag",    "sse41", "128", "16", 0, 0},
        {sw_diag_sse41_128_8,       "sw", "diag",    "sse41", "128", "8",  0, 0},
        {sw_striped_sse41_128_32,   "sw", "striped", "sse41", "128", "32", 0, 0},
        {sw_striped_sse41_128_16,   "sw", "striped", "sse41", "128", "16", 0, 0},
        {sw_striped_sse41_128_8,    "sw", "striped", "sse41", "128", "8",  0, 0},
#endif
                                   
        {nw_table,                  "nw", "",     "",      "",    "",   1, 1},
        {nw_table_scan,             "nw", "scan", "",      "",    "",   1, 0},
#if HAVE_SSE2
        {nw_table_scan_sse2_128_32,    "nw", "scan",    "sse2",  "128", "32", 1, 0},
        {nw_table_scan_sse2_128_16,    "nw", "scan",    "sse2",  "128", "16", 1, 0},
        {nw_table_scan_sse2_128_8,     "nw", "scan",    "sse2",  "128", "8",  1, 0},
        {nw_table_diag_sse2_128_32,    "nw", "diag",    "sse2",  "128", "32", 1, 0},
        {nw_table_diag_sse2_128_16,    "nw", "diag",    "sse2",  "128", "16", 1, 0},
        {nw_table_diag_sse2_128_8,     "nw", "diag",    "sse2",  "128", "8",  1, 0},
        {nw_table_striped_sse2_128_32, "nw", "striped", "sse2",  "128", "32", 1, 0},
        {nw_table_striped_sse2_128_16, "nw", "striped", "sse2",  "128", "16", 1, 0},
        {nw_table_striped_sse2_128_8,  "nw", "striped", "sse2",  "128", "8",  1, 0},
#endif
#if HAVE_SSE41
        {nw_table_scan_sse41_128_32,    "nw", "scan",    "sse41", "128", "32", 1, 0},
        {nw_table_scan_sse41_128_16,    "nw", "scan",    "sse41", "128", "16", 1, 0},
        {nw_table_scan_sse41_128_8,     "nw", "scan",    "sse41", "128", "8",  1, 0},
        {nw_table_diag_sse41_128_32,    "nw", "diag",    "sse41", "128", "32", 1, 0},
        {nw_table_diag_sse41_128_16,    "nw", "diag",    "sse41", "128", "16", 1, 0},
        {nw_table_diag_sse41_128_8,     "nw", "diag",    "sse41", "128", "8",  1, 0},
        {nw_table_striped_sse41_128_32, "nw", "striped", "sse41", "128", "32", 1, 0},
        {nw_table_striped_sse41_128_16, "nw", "striped", "sse41", "128", "16", 1, 0},
        {nw_table_striped_sse41_128_8,  "nw", "striped", "sse41", "128", "8",  1, 0},
#endif

        {sg_table,                  "sg", "",     "",     "",    "",   1, 1},
        {sg_table_scan,             "sg", "scan", "",     "",    "",   1, 0},
#if HAVE_SSE2
        {sg_table_scan_sse2_128_32,    "sg", "scan",    "sse2", "128", "32", 1, 0},
        {sg_table_scan_sse2_128_16,    "sg", "scan",    "sse2", "128", "16", 1, 0},
        {sg_table_scan_sse2_128_8,     "sg", "scan",    "sse2", "128", "8",  1, 0},
        {sg_table_diag_sse2_128_32,    "sg", "diag",    "sse2", "128", "32", 1, 0},
        {sg_table_diag_sse2_128_16,    "sg", "diag",    "sse2", "128", "16", 1, 0},
        {sg_table_diag_sse2_128_8,     "sg", "diag",    "sse2", "128", "8",  1, 0},
        {sg_table_striped_sse2_128_32, "sg", "striped", "sse2", "128", "32", 1, 0},
        {sg_table_striped_sse2_128_16, "sg", "striped", "sse2", "128", "16", 1, 0},
        {sg_table_striped_sse2_128_8,  "sg", "striped", "sse2", "128", "8",  1, 0},
#endif
#if HAVE_SSE41
        {sg_table_scan_sse41_128_32,   "sg", "scan",    "sse41", "128", "32", 1, 0},
        {sg_table_scan_sse41_128_16,   "sg", "scan",    "sse41", "128", "16", 1, 0},
        {sg_table_scan_sse41_128_8,    "sg", "scan",    "sse41", "128", "8",  1, 0},
        {sg_table_diag_sse41_128_32,   "sg", "diag",    "sse41", "128", "32", 1, 0},
        {sg_table_diag_sse41_128_16,   "sg", "diag",    "sse41", "128", "16", 1, 0},
        {sg_table_diag_sse41_128_8,    "sg", "diag",    "sse41", "128", "8",  1, 0},
        {sg_table_striped_sse41_128_32,"sg", "striped", "sse41", "128", "32", 1, 0},
        {sg_table_striped_sse41_128_16,"sg", "striped", "sse41", "128", "16", 1, 0},
        {sg_table_striped_sse41_128_8, "sg", "striped", "sse41", "128", "8",  1, 0},
#endif

        {sw_table,                  "sw", "",     "",     "",    "",   1, 1},
        {sw_table_scan,             "sw", "scan", "",     "",    "",   1, 0},
#if HAVE_SSE2
        {sw_table_scan_sse2_128_32,    "sw", "scan",    "sse2", "128", "32", 1, 0},
        {sw_table_scan_sse2_128_16,    "sw", "scan",    "sse2", "128", "16", 1, 0},
        {sw_table_scan_sse2_128_8,     "sw", "scan",    "sse2", "128", "8",  1, 0},
        {sw_table_diag_sse2_128_32,    "sw", "diag",    "sse2", "128", "32", 1, 0},
        {sw_table_diag_sse2_128_16,    "sw", "diag",    "sse2", "128", "16", 1, 0},
        {sw_table_diag_sse2_128_8,     "sw", "diag",    "sse2", "128", "8",  1, 0},
        {sw_table_striped_sse2_128_32, "sw", "striped", "sse2", "128", "32", 1, 0},
        {sw_table_striped_sse2_128_16, "sw", "striped", "sse2", "128", "16", 1, 0},
        {sw_table_striped_sse2_128_8,  "sw", "striped", "sse2", "128", "8",  1, 0},
#endif
#if HAVE_SSE41
        {sw_table_scan_sse41_128_32,    "sw", "scan",    "sse41", "128", "32", 1, 0},
        {sw_table_scan_sse41_128_16,    "sw", "scan",    "sse41", "128", "16", 1, 0},
        {sw_table_scan_sse41_128_8,     "sw", "scan",    "sse41", "128", "8",  1, 0},
        {sw_table_diag_sse41_128_32,    "sw", "diag",    "sse41", "128", "32", 1, 0},
        {sw_table_diag_sse41_128_16,    "sw", "diag",    "sse41", "128", "16", 1, 0},
        {sw_table_diag_sse41_128_8,     "sw", "diag",    "sse41", "128", "8",  1, 0},
        {sw_table_striped_sse41_128_32, "sw", "striped", "sse41", "128", "32", 1, 0},
        {sw_table_striped_sse41_128_16, "sw", "striped", "sse41", "128", "16", 1, 0},
        {sw_table_striped_sse41_128_8,  "sw", "striped", "sse41", "128", "8",  1, 0},
#endif

        {nw_stats,                  "nw_stats", "",     "",     "",    "",   0, 1},
        {nw_stats_scan,             "nw_stats", "scan", "",     "",    "",   0, 0},
                                   
        {sg_stats,                  "sg_stats", "",     "",     "",    "",   0, 1},
        {sg_stats_scan,             "sg_stats", "scan", "",     "",    "",   0, 0},
                                   
        {sw_stats,                  "sw_stats", "",     "",     "",    "",   0, 1},
        {sw_stats_scan,             "sw_stats", "scan", "",     "",    "",   0, 0},
                                   
        {nw_stats_table,            "nw_stats", "",     "",     "",    "",   1, 1},
        {nw_stats_table_scan,       "nw_stats", "scan", "",     "",    "",   1, 0},

        {sg_stats_table,            "sg_stats", "",     "",     "",    "",   1, 1},
        {sg_stats_table_scan,       "sg_stats", "scan", "",     "",    "",   1, 0},

        {sw_stats_table,            "sw_stats", "",     "",     "",    "",   1, 1},
        {sw_stats_table_scan,       "sw_stats", "scan", "",     "",    "",   1, 0},

        {NULL, "", "", "", "", "", 0, 0}
    };

    timer_init();

    printf("%-15s %8s %6s %4s %5s %8s %8s %8s %8s %5s %8s %8s %8s\n",
            "name", "type", "isa", "bits", "width",
            "score", "matches", "length",
            "avg", "imp", "stddev", "min", "max");

    index = 0;
    f = functions[index++];
    while (f.f) {
        char name[16];
        stats_clear(&stats_rdtsc);
        timer_rdtsc = timer_start();
        timer_nsecs = timer_real();
        for (i=0; i<limit; ++i) {
            timer_rdtsc_single = timer_start();
            timer_nsecs_single = timer_real();
            result = f.f(seqA, lena, seqB, lenb, 10, 1, blosum62);
            timer_rdtsc_single = timer_end(timer_rdtsc_single);
            timer_nsecs_single = timer_real() - timer_nsecs_single;
            stats_sample_value(&stats_rdtsc, timer_rdtsc_single);
            stats_sample_value(&stats_nsecs, timer_nsecs_single);
            score = result->score;
            matches = result->matches;
            length = result->length;
            parasail_result_free(result);
        }
        timer_rdtsc = timer_end(timer_rdtsc);
        timer_nsecs = timer_real() - timer_nsecs;
        if (f.is_ref) {
            timer_rdtsc_ref_mean = stats_rdtsc._mean;
            timer_nsecs_ref_mean = stats_nsecs._mean;
        }
        strcpy(name, f.alg);
        if (f.is_table) {
            char *und = strrchr(f.alg, '_');
            int is_stats = (und && !strcmp(und, "_stats"));
            char suffix[256] = {0};
            if (strlen(f.type)) {
                strcat(suffix, "_");
                strcat(suffix, f.type);
            }
            if (strlen(f.isa)) {
                strcat(suffix, "_");
                strcat(suffix, f.isa);
            }
            if (strlen(f.bits)) {
                strcat(suffix, "_");
                strcat(suffix, f.bits);
            }
            if (strlen(f.width)) {
                strcat(suffix, "_");
                strcat(suffix, f.width);
            }
            strcat(suffix, ".txt");
            result = f.f(seqA, lena, seqB, lenb, 10, 1, blosum62);
            {
                char filename[256];
                strcpy(filename, f.alg);
                strcat(filename, "_scr");
                strcat(filename, suffix);
                print_array(filename, result->score_table, seqA, lena, seqB, lenb);
            }
            if (is_stats) {
                char filename[256];
                strcpy(filename, f.alg);
                strcat(filename, "_mch");
                strcat(filename, suffix);
                print_array(filename, result->matches_table, seqA, lena, seqB, lenb);
            }
            if (is_stats) {
                char filename[256];
                strcpy(filename, f.alg);
                strcat(filename, "_len");
                strcat(filename, suffix);
                print_array(filename, result->length_table, seqA, lena, seqB, lenb);
            }
            parasail_result_free(result);
            strcat(name, "_table");
        }
#if USE_TIMER_REAL
        printf("%-15s %8s %6s %4s %5s %8d %8d %8d %8.2f %5.2f %8.2f %8.2f %8.2f %8.7f %5.7f %8.7f %8.7f %8.7f\n",
                name, f.type, f.isa, f.bits, f.width,
                score, matches, length,
                stats_rdtsc._mean, pctf(timer_rdtsc_ref_mean, stats_rdtsc._mean),
                stats_stddev(&stats_rdtsc), stats_rdtsc._min, stats_rdtsc._max,
                stats_nsecs._mean, pctf(timer_nsecs_ref_mean, stats_nsecs._mean),
                stats_stddev(&stats_nsecs), stats_nsecs._min, stats_nsecs._max);
#else
        printf("%-15s %8s %6s %4s %5s %8d %8d %8d %8.2f %5.2f %8.2f %8.2f %8.2f\n",
                name, f.type, f.isa, f.bits, f.width,
                score, matches, length,
                stats_rdtsc._mean, pctf(timer_rdtsc_ref_mean, stats_rdtsc._mean),
                stats_stddev(&stats_rdtsc), stats_rdtsc._min, stats_rdtsc._max);
#endif
        f = functions[index++];
    }

    return 0;
}
