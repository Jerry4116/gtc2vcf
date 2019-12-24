/* The MIT License

   Copyright (c) 2018-2019 Giulio Genovese

   Author: Giulio Genovese <giulio.genovese@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

 */

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

static inline FILE *get_file_handle(const char *str)
{
    if ( !str ) return NULL;
    FILE *ret;
    if ( strcmp(str, "-") == 0 )
        ret = stdout;
    else
    {
        ret = fopen(str, "w");
        if ( !ret ) error("Failed to open %s: %s\n", str, strerror(errno));
    }
    return ret;
}

static inline void flank2fasta(const char *name,
                               const char *flank,
                               FILE *stream)
{
    if ( !flank ) return;
    const char *left = strchr(flank, '[');
    const char *middle = strchr(flank, '/');
    const char *right = strchr(flank, ']');
    if ( !left || !middle || !right ) error("Flank sequence is malformed: %s\n", flank);

    fprintf(stream, "@%s:1\n", name);
    if ( *(middle-1) == '-' ) fprintf(stream, "%.*s%s\n", (int)(left - flank), flank, right + 1);
    else fprintf(stream, "%.*s%.*s%s\n", (int)(left - flank), flank, (int)(middle - left) - 1, left + 1, right + 1);
    fprintf(stream, "@%s:2\n", name);
    if ( *(middle-1) == '-' ) fprintf(stream, "%.*s%.*s%s\n", (int)(left - flank), flank, (int)(right - middle) - 1, middle + 1, right + 1);
    else fprintf(stream, "%.*s%.*s%s\n", (int)(left - flank), flank, (int)(right - middle) - 1, middle + 1, right + 1);
}

static inline int bcf_hdr_name2id_flexible(const bcf_hdr_t *hdr, char *chr)
{
    const char *ucsc[] = {"chr1", "chr2", "chr3", "chr4", "chr5", "chr6", "chr7", "chr8", "chr9", "chr10", "chr11",
                 "chr12", "chr13", "chr14", "chr15", "chr16", "chr17", "chr18", "chr19", "chr20", "chr21", "chr22"};
    int rid = bcf_hdr_name2id(hdr, chr);
    if ( rid >= 0 ) return rid;
    int num = strtol(chr, NULL, 0);
    if ( num > 22 ) rid = -1;
    else if ( num > 0 ) rid = bcf_hdr_name2id(hdr, ucsc[num-1]);
    else if ( strcmp(chr, "X") == 0 || strcmp(chr, "XY") == 0 || strcmp(chr, "XX") == 0 )
    {
        rid = bcf_hdr_name2id(hdr, "X");
        if ( rid >= 0 ) return rid;
        rid = bcf_hdr_name2id(hdr, "chrX");
    }
    else if ( strcmp(chr, "Y") == 0 )
    {
        rid = bcf_hdr_name2id(hdr, "chrY");
    }
    else if ( strcmp(chr, "MT") == 0 )
    {
        rid = bcf_hdr_name2id(hdr, "chrM");
    }
    return rid;
}

static inline char revnt(char iupac)
{
    static const char iupac_complement[128] = {
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,'-',  0,'/',
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,'T','V','G','H',  0,  0,'C','D',  0,  0,'M',  0,'K','N',  0,
          0,  0,'Y','S','A',  0,'B','W',  0,'R',  0,']',  0,'[',  0,  0,
          0,'T','V','G','H',  0,  0,'C','D',  0,  0,'M',  0,'K','N',  0,
          0,  0,'Y','S','A',  0,'B','W',  0,'R',  0,  0,  0,  0,  0,  0,
    };
    if ( iupac > 127 ) return 0;
    return iupac_complement[(int)iupac];
}

#define MAX_LEN_ALLELE_A 8
static inline void flank_reverse_complement(char *flank)
{
    // swap alleles, but only if first allele is one base pair long
    char *left = strchr(flank, '[');
    char *middle = strchr(flank, '/');
    char *right = strchr(flank, ']');
    if ( !left || !middle || !right ) error("Flank sequence is malformed: %s\n", flank);

    char tmp[MAX_LEN_ALLELE_A];
    if ( middle - left - 1 > MAX_LEN_ALLELE_A ) error("Cannot swap alleles in flank sequence %s\n", flank);
    {
        char *ptr1 = tmp;
        char *ptr2 = left + 1;
        while ( ptr2 < middle )
        {
            *ptr1 = *ptr2;
            ptr1++;
            ptr2++;
        }
        ptr1 = left + 1;
        ptr2++;
        while ( ptr2 < right )
        {
            *ptr1 = *ptr2;
            ptr1++;
            ptr2++;
        }
        *ptr1 = '/';
        ptr1++;
        ptr2 = tmp;
        while ( ptr1 < right )
        {
            *ptr1 = *ptr2;
            ptr1++;
            ptr2++;
        }
    }

    size_t len = strlen(flank);
    for (size_t i=0; i<len/2; i++)
    {
        char tmp = flank[i];
        flank[i] = revnt( flank[len-i-1] );
        flank[len-i-1] = revnt( tmp );
    }
    if (len % 2 == 1) flank[len/2] = revnt( flank[len/2] );
}

// this is the weird way Illumina left shifts indels
static inline void flank_left_shift(char *flank)
{
    char *left = strchr(flank, '[');
    char *middle = strchr(flank, '/');
    char *right = strchr(flank, ']');
    if ( !left || !middle || !right ) error("Flank sequence is malformed: %s\n", flank);

    int len = (int)(right - middle) - 1;
    while( ( left - flank >= len ) && ( strncmp( left - len, middle + 1, len ) == 0 ) )
    {
        for (char *ptr = left - len; ptr < right + 1 - len; ptr++) *ptr = *(ptr + len);
        left -= len;
        middle -= len;
        right -= len;
        for (char *ptr = middle + 1; ptr < right; ptr++) *(ptr + len + 1) = *ptr;
    }
}

// notice that this function can change the flank sequence, so the original should not be passed
static inline int get_position(htsFile *hts,
                               sam_hdr_t *sam_hdr,
                               bam1_t *b,
                               const char *name,
                               const char *flank,
                               int left_shift,
                               int *idx,
                               const char **chrom,
                               int *position,
                               int *strand)
{
    *idx = -1;
    const char *chrom_pair[2];
    int position_pair[2], strand_pair[2];
    int64_t aln_score_pair[2];
    int ret;
    while ( *idx < 1 && ( ret = sam_read1(hts, sam_hdr, b) ) >= 0 )
    {
        const char *qname = bam_get_qname(b);
        if ( b->core.flag & BAM_FSECONDARY || b->core.flag & BAM_FSUPPLEMENTARY ) continue;
        int qname_l = strlen(qname);
        if ( strncmp( qname, name, qname_l - 2 ) != 0 )
            error("Query ID %.*s found in SAM file but %s expected\n", qname_l - 2, qname, name );
        *idx = qname[qname_l - 1] == '1' ? 0 : ( qname[qname_l - 1] == '2' ? 1 : -1 );
        if ( *idx == -1 ) error("Query ID %s found in SAM file does not end with :1 or :2\n", qname);

        chrom_pair[*idx] = sam_hdr_tid2name( sam_hdr, b->core.tid );
        if ( !chrom_pair[*idx] ) chrom_pair[*idx] = "---";
        position_pair[*idx] = 0;
        strand_pair[*idx] = -1;
        if ( !(b->core.flag & BAM_FUNMAP) )
        {
            strand_pair[*idx] = bam_is_rev(b);
            int n_cigar = b->core.n_cigar;
            const uint32_t *cigar = bam_get_cigar(b);
            position_pair[*idx] = b->core.pos;

            const char *left = strchr(flank, '[');
            const char *middle = strchr(flank, '/');
            const char *right = strchr(flank, ']');
            if ( !left || !middle || !right ) error("Flank sequence is malformed: %s\n", flank);
            int qlen = bam_is_rev(b) ? strlen(flank) - (right - flank) : left - flank + 1;
            if ( left_shift && strchr( flank, '-' ) )
            {
                int len = (int)(right - middle) - 1;
                if ( bam_is_rev(b) )
                {
                    const char *ptr = right + 1;
                    while( strncmp( middle + 1, ptr, len ) == 0 ) { qlen -= len; ptr += len; }
                }
                else
                {
                    const char *ptr = left - len;
                    while( ptr >= flank && ( strncmp( ptr, middle + 1, len ) == 0 ) ) { qlen -= len; ptr -= len; }
                }
            }
            if ( strchr( flank, '-' ) && *idx == 0 ) qlen--;

            for (int k = 0; k < n_cigar && qlen > 1; k++)
            {
                int type = bam_cigar_type(bam_cigar_op(cigar[k]));
                int len = bam_cigar_oplen(cigar[k]);
                if ( ( type & 1 ) && ( type & 2 ) ) // consume reference sequence ( case M )
                {
                    position_pair[*idx] += min(len, qlen);
                    qlen -= len;
                }
                else if ( type & 1 ) // consume query sequence ( case I )
                {
                    qlen -= len;
                    if ( qlen <= 0 ) // we skipped the base pair that needed to be localized
                    {
                        position_pair[*idx] = 0;
                    }
                }
                else if ( type & 2 )
                {
                    position_pair[*idx] += len; // consume reference sequence ( case D )
                }
            }
            if ( qlen == 1 ) position_pair[*idx]++;
        }
        uint8_t *as = bam_aux_get( b, "AS" );
        aln_score_pair[*idx] = bam_aux2i(as);
    }
    if ( ret < -1 ) return -1;

    if ( aln_score_pair[0] == aln_score_pair[1] && position_pair[0] != position_pair[1] && position_pair[0] != 0 && position_pair[1] != 0 )
    {
        *position = 0;
    }
    else
    {
        *idx = aln_score_pair[1] > aln_score_pair[0];
        *chrom = chrom_pair[*idx];
        *position = position_pair[*idx];
        *strand = strand_pair[*idx];
    }

    if ( *position == 0 )
    {
        *idx = -1;
        fprintf(stderr, "Unable to determine position for marker %s\n", name);
    }
    return 0;
}

static inline char get_ref_base(faidx_t *fai,
                                const bcf_hdr_t *hdr,
                                bcf1_t *rec)
{
    int len;
    char *ref = faidx_fetch_seq(fai, bcf_seqname(hdr, rec), rec->pos, rec->pos, &len);
    if ( !ref ) error("faidx_fetch_seq failed at %s:%"PRId64"\n", bcf_seqname(hdr, rec), rec->pos + 1);
    char ref_base = ref[0];
    free(ref);
    return ref_base;
}

static inline void strupper(char *str) {
    char *s = str;
    while (*s) {
        *s = toupper((unsigned char) *s);
        s++;
     }
}

static inline int len_common_suffix(const char *s1,
                                    const char *s2,
                                    size_t n)
{
    int ret = 0;
    while (ret < n && *s1 == *s2)
    {
        s1--;
        s2--;
        ret++;
    }
    return ret;
}

static inline int len_common_prefix(const char *s1,
                                    const char *s2,
                                    size_t n)
{
    int ret = 0;
    while (ret < n && *s1 == *s2)
    {
        s1++;
        s2++;
        ret++;
    }
    return ret;
}

// see BPMRecord.py from https://github.com/Illumina/GTCtoVCF
static inline int get_indel_alleles(const char *flank,
                                    faidx_t *fai,
                                    const char *seqname,
                                    hts_pos_t pos,
                                    int allele_b_is_del,
                                    char *ref_base,
                                    kstring_t *allele_a,
                                    kstring_t *allele_b)
{
    const char *left = strchr(flank, '[');
    const char *middle = strchr(flank, '/');
    const char *right = strchr(flank, ']');
    if ( !left || !middle || !right ) error("Flank sequence is malformed: %s\n", flank);

    int len;
    char *ref = faidx_fetch_seq(fai, seqname,
         pos - (int)(left - flank), pos - 1 + (int)(right - middle) - 1 + strlen(right + 1), &len);
    if ( !ref ) error("faidx_fetch_seq failed at %s:%"PRId64"\n", seqname, pos + 1);
    strupper(ref);
    int del_left  = len_common_suffix(left - 1  , ref + (left - flank)                       , left - flank);
    int del_right = len_common_prefix(right + 1 , ref + (left - flank) + 1                   , strlen(right + 1));
    int ins_match =      0 == strncmp(middle + 1, ref + (left - flank)                       , right - middle - 1);
    int ins_left  = len_common_suffix(left - 1  , ref + (left - flank) - 1                   , left - flank);
    int ins_right = len_common_prefix(right + 1 , ref + (left - flank) + (right - middle) - 1, strlen(right + 1));
    int ref_is_del = ( del_left >= ins_left ) && ( del_right >= ins_right );
    if ( ( ref_is_del && del_left * del_right == 0 ) || ( !ref_is_del && ( !ins_match || ins_left * ins_right == 0 ) ) )
    {
        ref_is_del = -1;
        ref_base[0] = ref[left - flank];
        kputc(allele_b_is_del ? 'I' : 'D', allele_a);
        kputc(allele_b_is_del ? 'D' : 'I', allele_b);
    }
    else
    {
        ref_base[0] = ref[left - flank - 1 + ref_is_del];
        kputc(ref_base[0], allele_a);
        kputc(ref_base[0], allele_b);
        ksprintf(allele_b_is_del ? allele_a : allele_b, "%.*s", (int)(right - middle) - 1, ref_is_del ? middle + 1 : ref + (left - flank));
    }
    free(ref);
    return ref_is_del;
}

static inline int get_allele_b_idx(char ref_base,
                                   char *allele_a,
                                   char *allele_b)
{
    if ( *allele_a == 'D' || *allele_a == 'I' || *allele_b == 'D' || *allele_b == 'I' ) return 1;

    if ( *allele_a == ref_base ) return 1;
    else if ( *allele_b == ref_base ) return 0;
    else if ( *allele_a == '.' )
    {
        *allele_a = ref_base;
        return 1;
    }
    else if ( *allele_b == '.' )
    {
        *allele_b = ref_base;
        return 0;
    }
    return 2;
}

static inline int get_allele_a_idx(int allele_b_idx)
{
    switch ( allele_b_idx )
    {
        case 0:
            return 1;
        case 1:
            return 0;
        case 2:
            return 1;
        default:
            return -1;
    }
}

static inline int alleles_ab_to_vcf(const char **alleles,
                                    const char *ref_base,
                                    const char *allele_a,
                                    const char *allele_b,
                                    int allele_b_idx)
{
    switch ( allele_b_idx )
    {
        case -1:
            alleles[0] = ref_base;
            return 1;
        case 0:
            alleles[0] = allele_b;
            if ( allele_a[0] == '.' ) return 1;
            alleles[1] = allele_a;
            return 2;
        case 1:
            alleles[0] = allele_a;
            if ( allele_b[0] == '.' ) return 1;
            alleles[1] = allele_b;
            return 2;
        case 2:
            alleles[0] = ref_base;
            alleles[1] = allele_a;
            alleles[2] = allele_b;
            return 3;
        default:
            return -1;
    }
}
