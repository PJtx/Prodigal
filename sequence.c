/******************************************************************************
    PRODIGAL (PROkaryotic DynamIc Programming Genefinding ALgorithm)
    Copyright (C) 2007-2014 University of Tennessee / UT-Battelle

    Code Author:  Doug Hyatt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "sequence.h"

/******************************************************************************
  Read the sequence for training purposes.  If we encounter multiple
  sequences, we insert gaps in between each one to allow training on partial
  genes.  When we hit MAX_SEQ bp, we stop and return what we've got so
  far for training.  This routine reads in FASTA, and has a very 'loose'
  Genbank and Embl parser, but, to be safe, FASTA should generally be
  preferred.
******************************************************************************/

int read_seq_training(FILE *fp, unsigned char *seq, unsigned char *useq,
                      double *gc, int closed, int *num_seq)
{
  char line[MAX_LINE+1];
  int hdr = 0, fhdr = 0, bctr = 0, len = 0, wrn = 0;
  int gc_cont = 0;
  int i, gapsize = 0;
  double acl = 0.0;

  line[MAX_LINE] = '\0';
  while (fgets(line, MAX_LINE, fp) != NULL)
  {
    if (hdr == 0 && line[strlen(line)-1] != '\n' && wrn == 0)
    {
      wrn = 1;
      fprintf(stderr, "\n\nWarning: saw non-sequence line longer than ");
      fprintf(stderr, "%d chars, sequence might not be read ", MAX_LINE);
      fprintf(stderr, "correctly.\n\n");
    }
    if (line[0] == '>' || (line[0] == 'S' && line[1] == 'Q') ||
        (strlen(line) > 6 && strncmp(line, "ORIGIN", 6) == 0))
    {
      hdr = 1;
      if (fhdr > 0 && closed == 0)
      {
        for (i = 0; i < 8; i++)
        {
          set(useq, len);
          bctr+=2;
          len++;
        }
      }
      fhdr++;
    }
    else if (hdr == 1 && (line[0] == '/' && line[1] == '/'))
    {
      hdr = 0;
    }
    else if (hdr == 1)
    {
      if (strstr(line, "Expand") != NULL && strstr(line, "gap") != NULL)
      {
        sscanf(strstr(line, "gap")+4, "%d", &gapsize);
        if (gapsize < 1 || gapsize > MAX_LINE)
        {
          fprintf(stderr, "Error: gap size in gbk file can't exceed line");
          fprintf(stderr, " size.\n");
          exit(14);
        }
        for (i = 0; i < gapsize; i++)
        {
          line[i] = 'n';
        }
        line[i] = '\0';
      }
      for (i = 0; i < (int)strlen(line); i++)
      {
        if (line[i] < 'A' || line[i] > 'z')
        {
          continue;
        }
        if (line[i] == 'g' || line[i] == 'G')
        {
          set(seq, bctr);
          gc_cont++;
        }
        else if (line[i] == 't' || line[i] == 'T' ||
                 line[i] == 'u' || line[i] == 'U')
        {
          set(seq, bctr);
          set(seq, bctr+1);
        }
        else if (line[i] == 'c' || line[i] == 'C')
        {
          set(seq, bctr+1);
          gc_cont++;
        }
        else if (line[i] != 'a' && line[i] != 'A')
        {
          set(seq, bctr+1);
          set(useq, len);
        }
        bctr+=2;
        len++;
      }
    }
    if (len+MAX_LINE >= MAX_SEQ)
    {
      fprintf(stderr, "\n\nWarning: Sequence is long (max %d for training).\n",
              MAX_SEQ);
      fprintf(stderr, "Training on the first %d bases.\n\n", MAX_SEQ);
      break;
    }
  }
  if (fhdr > 1 && closed == 0)
  {
    for (i = 0; i < 8; i++)
    {
      set(useq, len);
      bctr+=2;
      len++;
    }
  }
  *gc = ((double)gc_cont / (double)len);
  acl = ((double)(len - (fhdr-1)*8))/(double)fhdr;

  /* Exit if there are errors, warn if sequence is small */
  if (len == 0)
  {
    fprintf(stderr, "\n\nSequence read failed (file must be Fasta, ");
    fprintf(stderr, "Genbank, or EMBL format).\n\n");
    exit(9);
  }
  if (len < MIN_SINGLE_GENOME)
  {
    fprintf(stderr, "\n\nError: Sequence must be %d", MIN_SINGLE_GENOME);
    fprintf(stderr, " characters (only %d read).\n(Consider", len);
    fprintf(stderr, " running with the '-p anon' option or finding");
    fprintf(stderr, " more contigs from the same genome.)\n\n");
    exit(10);
  }
  if (len < IDEAL_SINGLE_GENOME)
  {
    fprintf(stderr, "\n\nWarning: Ideally Prodigal should be given at");
    fprintf(stderr, " least %d bases for ", IDEAL_SINGLE_GENOME);
    fprintf(stderr, "training.\nYou may get better results with the ");
    fprintf(stderr, "'-p anon' option.\n\n");
  }
  if (acl < IDEAL_AVG_CONTIG_LEN)
  {
    fprintf(stderr, "\n\nWarning: Average training set contig length is");
    fprintf(stderr, " short at %.2f bases.\nYou may", acl);
    fprintf(stderr, " get better results with the '-p anon' option.\n\n");
  }
  *num_seq = fhdr;
  return len;
}

/* This routine reads in the next sequence in a FASTA/GB/EMBL file */

int next_seq_multi(FILE *fp, unsigned char *seq, unsigned char *useq,
                   int *sctr, double *gc, char *cur_hdr, char *new_hdr)
{
  char line[MAX_LINE+1];
  int reading_seq = 0, genbank_end = 0, bctr = 0, len = 0, wrn = 0;
  int gc_cont = 0;
  int i, gapsize = 0;

  sprintf(new_hdr, "Prodigal_Seq_%d", *sctr+2);

  if (*sctr > 0)
  {
    reading_seq = 1;
  }
  line[MAX_LINE] = '\0';
  while (fgets(line, MAX_LINE, fp) != NULL)
  {
    if (reading_seq == 0 && line[strlen(line)-1] != '\n' && wrn == 0)
    {
      wrn = 1;
      fprintf(stderr, "\n\nWarning: saw non-sequence line longer than ");
      fprintf(stderr, "%d chars, sequence might not be read ", MAX_LINE);
      fprintf(stderr, "correctly.\n\n");
    }
    if (strlen(line) > 10 && strncmp(line, "DEFINITION", 10) == 0)
    {
      if (genbank_end == 0)
      {
        strcpy(cur_hdr, line+12);
        cur_hdr[strlen(cur_hdr)-1] = '\0';
      }
      else
      {
        strcpy(new_hdr, line+12);
        new_hdr[strlen(new_hdr)-1] = '\0';
      }
    }
    if (line[0] == '>' || (line[0] == 'S' && line[1] == 'Q') ||
        (strlen(line) > 6 && strncmp(line, "ORIGIN", 6) == 0))
    {
      if (reading_seq == 1 || genbank_end == 1 || *sctr > 0)
      {
        if (line[0] == '>')
        {
          strcpy(new_hdr, line+1);
          new_hdr[strlen(new_hdr)-1] = '\0';
        }
        break;
      }
      if (line[0] == '>')
      {
        strcpy(cur_hdr, line+1);
        cur_hdr[strlen(cur_hdr)-1] = '\0';
      }
      reading_seq = 1;
    }
    else if (reading_seq == 1 && (line[0] == '/' && line[1] == '/'))
    {
      reading_seq = 0;
      genbank_end = 1;
    }
    else if (reading_seq == 1)
    {
      if (strstr(line, "Expand") != NULL && strstr(line, "gap") != NULL)
      {
        sscanf(strstr(line, "gap")+4, "%d", &gapsize);
        if (gapsize < 1 || gapsize > MAX_LINE)
        {
          fprintf(stderr, "Error: gap size in gbk file can't exceed line");
          fprintf(stderr, " size.\n");
          exit(15);
        }
        for (i = 0; i < gapsize; i++)
        {
          line[i] = 'n';
        }
        line[i] = '\0';
      }
      for (i = 0; i < (int)strlen(line); i++)
      {
        if (line[i] < 'A' || line[i] > 'z')
        {
          continue;
        }
        if (line[i] == 'g' || line[i] == 'G')
        {
          set(seq, bctr);
          gc_cont++;
        }
        else if (line[i] == 't' || line[i] == 'T' ||
                 line[i] == 'u' || line[i] == 'U')
        {
          set(seq, bctr);
          set(seq, bctr+1);
        }
        else if (line[i] == 'c' || line[i] == 'C')
        {
          set(seq, bctr+1);
          gc_cont++;
        }
        else if (line[i] != 'a' && line[i] != 'A')
        {
          set(seq, bctr+1);
          set(useq, len);
        }
        bctr+=2;
        len++;
      }
    }
    if (len+MAX_LINE >= MAX_SEQ)
    {
      fprintf(stderr, "Sequence too long (max %d permitted).\n", MAX_SEQ);
      exit(16);
    }
  }
  if (len == 0)
  {
    return -1;
  }
  *gc = ((double)gc_cont / (double)len);
  *sctr = *sctr + 1;
  return len;
}

/* Takes first word of header */
void calc_short_header(char *header, char *short_header, int sctr)
{
  int i;

  strcpy(short_header, header);
  for (i = 0; i < (int)strlen(header); i++)
  {
    if (header[i] == ' ' || header[i] == '\t' || header[i] == '\r' ||
        header[i] == '\n')
    {
      strncpy(short_header, header, i);
      short_header[i] = '\0';
      break;
    }
  }
  if (i == 0)
  {
    sprintf(short_header, "Prodigal_Seq_%d", sctr);
  }
}

/* Takes rseq and fills it up with the rev complement of seq */

void reverse_seq(unsigned char *seq, unsigned char *rseq, unsigned char *useq,
                 int len)
{
  int i, slen=len*2;
  for (i = 0; i < slen; i++)
  {
    if (test(seq, i) == 0)
    {
       set(rseq, slen-i-1+(i%2==0?-1:1));
    }
  }
  for (i = 0; i < len; i++)
  {
    if (test(useq, i) == 1)
    {
      toggle(rseq, slen-1-i*2);
      toggle(rseq, slen-2-i*2);
    }
  }
}

/* Simple routines to say whether or not bases are */
/* a, c, t, g, starts, stops, etc. */

int is_a(unsigned char *seq, int n)
{
  int index = n*2;
  if (test(seq, index) == 1 || test(seq, index+1) == 1)
  {
    return 0;
  }
  return 1;
}

int is_c(unsigned char *seq, int n)
{
  int index = n*2;
  if (test(seq, index) == 1 || test(seq, index+1) == 0)
  {
    return 0;
  }
  return 1;
}

int is_g(unsigned char *seq, int n)
{
  int index = n*2;
  if (test(seq, index) == 0 || test(seq, index+1) == 1)
  {
     return 0;
  }
  return 1;
}

int is_t(unsigned char *seq, int n)
{
  int index = n*2;
  if (test(seq, index) == 0 || test(seq, index+1) == 0)
  {
    return 0;
  }
  return 1;
}

int is_n(unsigned char *useq, int n)
{
  if (test(useq, n) == 0)
  {
    return 0;
  }
  return 1;
}

int is_stop(unsigned char *seq, int n, int trans_table)
{

  /* TAG: Not a stop in genetic codes 6, 15, 16, and 22 */
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 6 || trans_table == 15 ||
        trans_table == 16 || trans_table == 22)
    {
       return 0;
    }
    return 1;
  }

  /* TGA: Not a stop in genetic codes 2-5, 9-10, 13-14, 21, 24-25 */
  if (is_t(seq, n) == 1 && is_g(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if ((trans_table >= 2 && trans_table <= 5) ||
        trans_table == 9 || trans_table == 10 ||
        trans_table == 13 || trans_table == 14 ||
        trans_table == 21 || trans_table == 24 ||
        trans_table == 25)
    {
      return 0;
    }
    return 1;
  }

  /* TAA: Not a stop in genetic codes 6 and 14 */
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 6 || trans_table == 14)
    {
      return 0;
    }
    return 1;
  }

  /* Code 2 */
  if (trans_table == 2 && is_a(seq, n) == 1 && is_g(seq, n+1) == 1 &&
      is_a(seq, n+2) == 1)
  {
    return 1;
  }
  if (trans_table == 2 && is_a(seq, n) == 1 && is_g(seq, n+1) == 1 &&
      is_g(seq, n+2) == 1)
  {
    return 1;
  }

  /* Code 22 */
  if (trans_table == 22 && is_t(seq, n) == 1 && is_c(seq, n+1) == 1 &&
      is_a(seq, n+2) == 1)
  {
    return 1;
  }

  /* Code 23 */
  if (trans_table == 23 && is_t(seq, n) == 1 && is_t(seq, n+1) == 1 &&
      is_a(seq, n+2) == 1)
  {
    return 1;
  }

  return 0;
}

/* Prodigal only supports ATG/GTG/TTG starts as 'standard' possibilities. */
/* Some genetic codes have other initiation codons listed, but we do not  */
/* support these. */
int is_start(unsigned char *seq, int n, int trans_table)
{

  /* ATG: Always a start codon */
  if (is_a(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 1;
  }

  /* GTG: Start codon in 2/4/5/9/11/13/21/23/24/25 */
  if (is_g(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 2 || trans_table == 4 ||
        trans_table == 5 || trans_table == 9 ||
        trans_table == 11 || trans_table == 13 ||
        trans_table == 21 || trans_table == 23 ||
        trans_table == 24 || trans_table == 25)
    {
      return 1;
    }
  }

  /* TTG: Start codon in 4/5/11/13/24/25 */
  if (is_t(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 4 || trans_table == 5 ||
        trans_table == 11 || trans_table == 13 ||
        trans_table == 24 || trans_table == 25)
    {
      return 1;
    }
  }

  /* We do not handle other initiation codons */
  return 0;
}

/* Routines to say if codons are common start or stop triplets */
int is_atg(unsigned char *seq, int n)
{
  if (is_a(seq, n) == 0 || is_t(seq, n+1) == 0 || is_g(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_gtg(unsigned char *seq, int n)
{
  if (is_g(seq, n) == 0 || is_t(seq, n+1) == 0 || is_g(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_ttg(unsigned char *seq, int n)
{
  if (is_t(seq, n) == 0 || is_t(seq, n+1) == 0 || is_g(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_taa(unsigned char *seq, int n)
{
  if (is_t(seq, n) == 0 || is_a(seq, n+1) == 0 || is_a(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_tag(unsigned char *seq, int n)
{
  if (is_t(seq, n) == 0 || is_a(seq, n+1) == 0 || is_g(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_tga(unsigned char *seq, int n)
{
  if (is_t(seq, n) == 0 || is_g(seq, n+1) == 0 || is_a(seq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int is_nnn(unsigned char *useq, int n)
{
  if (is_n(useq, n) == 0 || is_n(useq, n+1) == 0 || is_n(useq, n+2) == 0)
  {
    return 0;
  }
  return 1;
}

int codon_has_n(unsigned char *useq, int n)
{
  if (is_n(useq, n) == 1 || is_n(useq, n+1) == 1 || is_n(useq, n+2) == 1)
  {
    return 1;
  }
  return 0;
}

int gap_to_left(unsigned char *useq, int n)
{
  if (is_nnn(useq, n-3) == 1 && is_nnn(useq, n-6) == 1)
  {
    return 1;
  }
  else if (is_n(useq, n-3) == 1 && is_nnn(useq, n-6) == 1 &&
           is_nnn(useq, n-9) == 1)
  {
    return 1;
  }
  return 0;
}

int gap_to_right(unsigned char *useq, int n)
{
  if (is_nnn(useq, n+3) == 1 && is_nnn(useq, n+6) == 1)
  {
    return 1;
  }
  else if (is_n(useq, n+5) == 1 && is_nnn(useq, n+6) == 1 &&
           is_nnn(useq, n+9) == 1)
  {
    return 1;
  }
  return 0;
}

int is_gc(unsigned char *seq, int n)
{
  int index = n*2;
  if (test(seq, index) != test(seq, index+1))
  {
    return 1;
  }
  return 0;
}

/* Returns the probability a random codon should be a stop codon */
/* based on the GC content and genetic code of the organism */
double prob_stop(int tt, double gc)
{
  int i1, i2, i3, i4, i5, i6;
  unsigned char codon[3];
  double cprob, stop_prob = 0.0;

  for (i1 = 0; i1 < 6; i1++)
  {
    clear(codon, i1);
  }
  for (i1 = 0; i1 < 2; i1++)
  {
    if (i1 == 0)
    {
      clear(codon, 0);
    }
    else
    {
      set(codon, 0);
    }
    for (i2 = 0; i2 < 2; i2++)
    {
      if (i2 == 0)
      {
        clear(codon, 1);
      }
      else
      {
        set(codon, 1);
      }
      for (i3 = 0; i3 < 2; i3++)
      {
        if (i3 == 0)
        {
          clear(codon, 2);
        }
        else
        {
          set(codon, 2);
        }
        for (i4 = 0; i4 < 2; i4++)
        {
          if (i4 == 0)
          {
            clear(codon, 3);
          }
          else
          {
            set(codon, 3);
          }
          for (i5 = 0; i5 < 2; i5++)
          {
            if (i5 == 0)
            {
              clear(codon, 4);
            }
            else
            {
              set(codon, 4);
            }
            for (i6 = 0; i6 < 2; i6++)
            {
              if (i6 == 0)
              {
                clear(codon, 5);
              }
              else
              {
                set(codon, 5);
              }
              cprob = 1.0;
              if (is_gc(codon, 0) == 1)
              {
                cprob *= gc/2.0;
              }
              else
              {
                cprob *= (1.0-gc)/2.0;
              }
              if (is_gc(codon, 1) == 1)
              {
                cprob *= gc/2.0;
              }
              else
              {
                cprob *= (1.0-gc)/2.0;
              }
              if (is_gc(codon, 2) == 1)
              {
                cprob *= gc/2.0;
              }
              else
              {
                cprob *= (1.0-gc)/2.0;
              }
              if (is_stop(codon, 0, tt) == 1)
              {
                stop_prob += cprob;
              }
            }
          }
        }
      }
    }
  }
  return stop_prob;
}

double gc_content(unsigned char *seq, int a, int b)
{
  double sum = 0.0, gc = 0.0;
  int i;
  for (i = a; i <= b; i++)
  {
    if (is_g(seq, i) == 1 || is_c(seq, i) == 1)
    {
      gc++;
    }
    sum++;
  }
  return gc/sum;
}

/* Returns a single amino acid for this position */
char amino(unsigned char *seq, int n, int trans_table, int is_init)
{
  if (is_stop(seq, n, trans_table) == 1)
  {
    return '*';
  }
  if (is_start(seq, n, trans_table) == 1 && is_init == 1)
  {
    return 'M';
  }
  if (is_t(seq, n) == 1 && is_t(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'F';
  }
  if (is_t(seq, n) == 1 && is_t(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'F';
  }
  if (is_t(seq, n) == 1 && is_t(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    return 'L';
  }
  if (is_t(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'L';
  }
  if (is_t(seq, n) == 1 && is_c(seq, n+1) == 1)
  {
    return 'S';
  }
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'Y';
  }
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'Y';
  }
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 6)
    {
      return 'Q';
    }
    if (trans_table == 14)
    {
      return 'Y';
    }
  }
  if (is_t(seq, n) == 1 && is_a(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 6 || trans_table == 15)
    {
      return 'Q';
    }
    if (trans_table == 16 || trans_table == 22)
    {
      return 'L';
    }
  }
  if (is_t(seq, n) == 1 && is_g(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'C';
  }
  if (is_t(seq, n) == 1 && is_g(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'C';
  }
  if (is_t(seq, n) == 1 && is_g(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 10)
    {
      return 'C';
    }
    if (trans_table == 25)
    {
      return 'G';
    }
    return 'W';
  }
  if (is_t(seq, n) == 1 && is_g(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'W';
  }
  if (is_c(seq, n) == 1 && is_t(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    if (trans_table == 3)
    {
      return 'T';
    }
    return 'L';
  }
  if (is_c(seq, n) == 1 && is_t(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    if (trans_table == 3)
    {
      return 'T';
    }
    return 'L';
  }
  if (is_c(seq, n) == 1 && is_t(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 3)
    {
      return 'T';
    }
    return 'L';
  }
  if (is_c(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 3)
    {
      return 'T';
    }
    if (trans_table == 12)
    {
      return 'S';
    }
    return 'L';
  }
  if (is_c(seq, n) == 1 && is_c(seq, n+1) == 1)
  {
    return 'P';
  }
  if (is_c(seq, n) == 1 && is_a(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'H';
  }
  if (is_c(seq, n) == 1 && is_a(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'H';
  }
  if (is_c(seq, n) == 1 && is_a(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    return 'Q';
  }
  if (is_c(seq, n) == 1 && is_a(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'Q';
  }
  if (is_c(seq, n) == 1 && is_g(seq, n+1) == 1)
  {
    return 'R';
  }
  if (is_a(seq, n) == 1 && is_t(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'I';
  }
  if (is_a(seq, n) == 1 && is_t(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'I';
  }
  if (is_a(seq, n) == 1 && is_t(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 2 || trans_table == 3 ||
        trans_table == 5 || trans_table == 13 ||
        trans_table == 21)
    {
      return 'M';
    }
    return 'I';
  }
  if (is_a(seq, n) == 1 && is_t(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'M';
  }
  if (is_a(seq, n) == 1 && is_c(seq, n+1) == 1)
  {
    return 'T';
  }
  if (is_a(seq, n) == 1 && is_a(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'N';
  }
  if (is_a(seq, n) == 1 && is_a(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'N';
  }
  if (is_a(seq, n) == 1 && is_a(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 9 || trans_table == 14 ||
        trans_table == 21)
    {
      return 'N';
    }
    return 'K';
  }
  if (is_a(seq, n) == 1 && is_a(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'K';
  }
  if (is_a(seq, n) == 1 && is_g(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'S';
  }
  if (is_a(seq, n) == 1 && is_g(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'S';
  }
  if (is_a(seq, n) == 1 && is_g(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    if (trans_table == 13)
    {
      return 'G';
    }
    if (trans_table == 5 || trans_table == 9 ||
        trans_table == 14 || trans_table == 21 ||
        trans_table == 24)
    {
      return 'S';
    }
    return 'R';
  }
  if (is_a(seq, n) == 1 && is_g(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    if (trans_table == 13)
    {
      return 'G';
    }
    if (trans_table == 5 || trans_table == 9 ||
        trans_table == 14 || trans_table == 21)
    {
      return 'S';
    }
    if (trans_table == 24)
    {
      return 'K';
    }
    return 'R';
  }
  if (is_g(seq, n) == 1 && is_t(seq, n+1) == 1)
  {
    return 'V';
  }
  if (is_g(seq, n) == 1 && is_c(seq, n+1) == 1)
  {
    return 'A';
  }
  if (is_g(seq, n) == 1 && is_a(seq, n+1) == 1 && is_t(seq, n+2) == 1)
  {
    return 'D';
  }
  if (is_g(seq, n) == 1 && is_a(seq, n+1) == 1 && is_c(seq, n+2) == 1)
  {
    return 'D';
  }
  if (is_g(seq, n) == 1 && is_a(seq, n+1) == 1 && is_a(seq, n+2) == 1)
  {
    return 'E';
  }
  if (is_g(seq, n) == 1 && is_a(seq, n+1) == 1 && is_g(seq, n+2) == 1)
  {
    return 'E';
  }
  if (is_g(seq, n) == 1 && is_g(seq, n+1) == 1)
  {
    return 'G';
  }
  return 'X';
}

/* Converts an amino acid letter to a numerical value */
int amino_num(char aa)
{
  if (aa == 'a' || aa == 'A')
  {
    return 0;
  }
  if (aa == 'c' || aa == 'C')
  {
    return 1;
  }
  if (aa == 'd' || aa == 'D')
  {
    return 2;
  }
  if (aa == 'e' || aa == 'E')
  {
    return 3;
  }
  if (aa == 'f' || aa == 'F')
  {
    return 4;
  }
  if (aa == 'g' || aa == 'G')
  {
    return 5;
  }
  if (aa == 'h' || aa == 'H')
  {
    return 6;
  }
  if (aa == 'i' || aa == 'I')
  {
    return 7;
  }
  if (aa == 'k' || aa == 'K')
  {
    return 8;
  }
  if (aa == 'l' || aa == 'L')
  {
    return 9;
  }
  if (aa == 'm' || aa == 'M')
  {
    return 10;
  }
  if (aa == 'n' || aa == 'N')
  {
    return 11;
  }
  if (aa == 'p' || aa == 'P')
  {
    return 12;
  }
  if (aa == 'q' || aa == 'Q')
  {
    return 13;
  }
  if (aa == 'r' || aa == 'R')
  {
    return 14;
  }
  if (aa == 's' || aa == 'S')
  {
    return 15;
  }
  if (aa == 't' || aa == 'T')
  {
    return 16;
  }
  if (aa == 'v' || aa == 'V')
  {
    return 17;
  }
  if (aa == 'w' || aa == 'W')
  {
    return 18;
  }
  if (aa == 'y' || aa == 'Y')
  {
    return 19;
  }
  return -1;
}

/* Converts a numerical value to an amino acid letter */
char amino_letter(int num)
{
  if (num == 0)
  {
    return 'A';
  }
  if (num == 1)
  {
    return 'C';
  }
  if (num == 2)
  {
    return 'D';
  }
  if (num == 3)
  {
    return 'E';
  }
  if (num == 4)
  {
    return 'F';
  }
  if (num == 5)
  {
    return 'G';
  }
  if (num == 6)
  {
    return 'H';
  }
  if (num == 7)
  {
    return 'I';
  }
  if (num == 8)
  {
    return 'K';
  }
  if (num == 9)
  {
    return 'L';
  }
  if (num == 10)
  {
    return 'M';
  }
  if (num == 11)
  {
    return 'N';
  }
  if (num == 12)
  {
    return 'P';
  }
  if (num == 13)
  {
    return 'Q';
  }
  if (num == 14)
  {
    return 'R';
  }
  if (num == 15)
  {
    return 'S';
  }
  if (num == 16)
  {
    return 'T';
  }
  if (num == 17)
  {
    return 'V';
  }
  if (num == 18)
  {
    return 'W';
  }
  if (num == 19)
  {
    return 'Y';
  }
  return 'X';
}

/* Returns the corresponding frame on the reverse strand */

int rframe(int fr, int slen)
{
  int md = slen%3-1;
  if (md == 0)
  {
    md = 3;
  }
  return (md-fr);
}

/* Simple 3-way max function */

int max_fr(int n1, int n2, int n3)
{
  if (n1 > n2)
  {
    if (n1 > n3)
    {
      return 0;
    }
    else
    {
      return 2;
    }
  }
  else
  {
    if (n2 > n3)
    {
      return 1;
    }
    else
    {
      return 2;
    }
  }
}

/******************************************************************************
  Creates a GC frame plot for a given sequence.  This is simply a string with
  the highest GC content frame in a window centered on a position for every
  position in the sequence.
******************************************************************************/

int *calc_most_gc_frame(unsigned char *seq, int slen)
{
  int i, j, *fwd, *bwd, *tot;
  int win, *gp;

  gp = (int *)malloc(slen*sizeof(double));
  fwd = (int *)malloc(slen*sizeof(int));
  bwd = (int *)malloc(slen*sizeof(int));
  tot = (int *)malloc(slen*sizeof(int));
  if (fwd == NULL || bwd == NULL || gp == NULL || tot == NULL)
  {
    perror("\n\nMalloc failed on GC frame plot.");
    exit(11);
  }
  for (i = 0; i < slen; i++)
  {
    fwd[i] = 0;
    bwd[i] = 0;
    tot[i] = 0;
    gp[i] = -1;
  }

  for (i = 0; i < 3; i++)
  {
    for (j = 0 + i; j < slen; j++)
    {
      if (j < 3)
      {
        fwd[j] = is_gc(seq, j);
      }
      else
      {
        fwd[j] = fwd[j-3] + is_gc(seq, j);
      }
      if (j < 3)
      {
        bwd[slen-j-1] = is_gc(seq, slen-j-1);
      }
      else
      {
        bwd[slen-j-1] = bwd[slen-j+2] + is_gc(seq, slen-j-1);
      }
    }
  }
  for (i = 0; i < slen; i++)
  {
    tot[i] = fwd[i] + bwd[i] - is_gc(seq, i);
    if (i - WINDOW/2 >= 0)
    {
      tot[i] -= fwd[i-WINDOW/2];
    }
    if (i + WINDOW/2 < slen)
    {
      tot[i] -= bwd[i+WINDOW/2];
    }
  }
  free(fwd);
  free(bwd);
  for (i = 0; i < slen-2; i+=3)
  {
    win = max_fr(tot[i], tot[i+1], tot[i+2]);
    for (j = 0; j < 3; j++)
    {
      gp[i+j] = win;
    }
  }
  free(tot);
  return gp;
}

/* Converts a word of size len to a number */
int mer_index(int len, unsigned char *seq, int pos)
{
  int i, index = 0;
  for (i = 0; i < 2*len; i++)
  {
    index |= (test(seq, pos*2+i)<<i);
  }
  return index;
}

/* Gives a text string for a start */
void start_text(char *st, int type)
{
  if (type == 0)
  {
    st[0] = 'A';
  }
  else if (type == 1)
  {
    st[0] = 'G';
  }
  else if (type == 2)
  {
    st[0] = 'T';
  }
  st[1] = 'T';
  st[2] = 'G';
  st[3] = '\0';
}

/* Gives a text string for a mer of len 'len' (useful in outputting motifs) */
void mer_text(char *qt, int len, int index)
{
  int i, val;
  char letters[4] = { 'A', 'G', 'C', 'T' };
  if (len == 0)
  {
    strcpy(qt, "None");
  }
  else
  {
    for (i = 0; i < len; i++)
    {
      val = (index&(1<<(2*i))) + (index&(1<<(2*i+1)));
      val >>= (i*2);
      qt[i] = letters[val];
    }
    qt[i] = '\0';
  }
}

/* Builds a 'len'-mer background for whole sequence */
void calc_mer_bg(int len, unsigned char *seq, unsigned char *rseq, int slen,
                 double *bg)
{
  int i, glob = 0, size = 1;
  int *counts;

  for (i = 1; i <= len; i++)
  {
    size *= 4;
  }
  counts = (int *)malloc(size * sizeof(int));
  for (i = 0; i < size; i++)
  {
    counts[i] = 0;
  }
  for (i = 0; i < slen-len+1; i++)
  {
    counts[mer_index(len, seq, i)]++;
    counts[mer_index(len, rseq, i)]++;
    glob+=2;
  }
  for (i = 0; i < size; i++)
  {
    bg[i] = (double)((counts[i]*1.0)/(glob*1.0));
  }
  free(counts);
}

/******************************************************************************
  Finds the highest-scoring region similar to AGGAGG in a given stretch of
  sequence upstream of a start.
******************************************************************************/

int shine_dalgarno_exact(unsigned char *seq, int pos, int start, double *rwt)
{
  int i, j, k, mism, rdis, limit, max_val, cur_val = 0;
  double match[6], cur_ctr, dis_flag;

  limit = imin(6, start-4-pos);
  for (i = limit; i < 6; i++)
  {
    match[i] = -10.0;
  }

  /* Compare the 6-base region to AGGAGG */
  for (i = 0; i < limit; i++)
  {
    if (pos+i < 0)
    {
      continue;
    }
    if (i%3 == 0 && is_a(seq, pos+i) == 1)
    {
      match[i] = 2.0;
    }
    else if (i%3 != 0 && is_g(seq, pos+i) == 1)
    {
      match[i] = 3.0;
    }
    else
    {
      match[i] = -10.0;
    }
  }

  /* Find the maximally scoring motif */
  max_val = 0;
  for (i = limit; i >= 3; i--)
  {
    for (j = 0; j <= limit-i; j++)
    {
      cur_ctr = -2.0;
      mism = 0;
      for (k = j; k < j+i; k++)
      {
        cur_ctr += match[k];
        if (match[k] < 0.0)
        {
          mism++;
        }
      }
      if (mism > 0)
      {
        continue;
      }
      rdis = start - (pos+j+i);
      if (rdis < 5 && i < 5)
      {
        dis_flag = 2;
      }
      else if (rdis < 5 && i >= 5)
      {
        dis_flag = 1;
      }
      else if (rdis > 10 && rdis <= 12 && i < 5)
      {
        dis_flag = 1;
      }
      else if (rdis > 10 && rdis <= 12 && i >= 5)
      {
        dis_flag = 2;
      }
      else if (rdis >= 13)
      {
        dis_flag = 3;
      }
      else
      {
        dis_flag = 0;
      }
      if (rdis > 15 || cur_ctr < 6.0)
      {
        continue;
      }

      /* Exact-Matching RBS Motifs */
      if (cur_ctr < 6.0)
      {
        cur_val = 0;
      }
      else if (cur_ctr == 6.0 && dis_flag == 2)
      {
        cur_val = 1;
      }
      else if (cur_ctr == 6.0 && dis_flag == 3)
      {
        cur_val = 2;
      }
      else if (cur_ctr == 8.0 && dis_flag == 3)
      {
        cur_val = 3;
      }
      else if (cur_ctr == 9.0 && dis_flag == 3)
      {
        cur_val = 3;
      }
      else if (cur_ctr == 6.0 && dis_flag == 1)
      {
        cur_val = 6;
      }
      else if (cur_ctr == 11.0 && dis_flag == 3)
      {
        cur_val = 10;
      }
      else if (cur_ctr == 12.0 && dis_flag == 3)
      {
        cur_val = 10;
      }
      else if (cur_ctr == 14.0 && dis_flag == 3)
      {
        cur_val = 10;
      }
      else if (cur_ctr == 8.0 && dis_flag == 2)
      {
        cur_val = 11;
      }
      else if (cur_ctr == 9.0 && dis_flag == 2)
      {
        cur_val = 11;
      }
      else if (cur_ctr == 8.0 && dis_flag == 1)
      {
        cur_val = 12;
      }
      else if (cur_ctr == 9.0 && dis_flag == 1)
      {
        cur_val = 12;
      }
      else if (cur_ctr == 6.0 && dis_flag == 0)
      {
        cur_val = 13;
      }
      else if (cur_ctr == 8.0 && dis_flag == 0)
      {
        cur_val = 15;
      }
      else if (cur_ctr == 9.0 && dis_flag == 0)
      {
        cur_val = 16;
      }
      else if (cur_ctr == 11.0 && dis_flag == 2)
      {
        cur_val = 20;
      }
      else if (cur_ctr == 11.0 && dis_flag == 1)
      {
        cur_val = 21;
      }
      else if (cur_ctr == 11.0 && dis_flag == 0)
      {
        cur_val = 22;
      }
      else if (cur_ctr == 12.0 && dis_flag == 2)
      {
        cur_val = 20;
      }
      else if (cur_ctr == 12.0 && dis_flag == 1)
      {
        cur_val = 23;
      }
      else if (cur_ctr == 12.0 && dis_flag == 0)
      {
        cur_val = 24;
      }
      else if (cur_ctr == 14.0 && dis_flag == 2)
      {
        cur_val = 25;
      }
      else if (cur_ctr == 14.0 && dis_flag == 1)
      {
        cur_val = 26;
      }
      else if (cur_ctr == 14.0 && dis_flag == 0)
      {
        cur_val = 27;
      }

      if (rwt[cur_val] < rwt[max_val])
      {
        continue;
      }
      if (rwt[cur_val] == rwt[max_val] && cur_val < max_val)
      {
        continue;
      }
      max_val = cur_val;
    }
  }

  return max_val;
}

/******************************************************************************
  Finds the highest-scoring region similar to AGGAGG in a given stretch of
  sequence upstream of a start.  Only considers 5/6-mers with 1 mismatch.
******************************************************************************/

int shine_dalgarno_mm(unsigned char *seq, int pos, int start, double *rwt)
{
  int i, j, k, mism, rdis, limit, max_val, cur_val = 0;
  double match[6], cur_ctr, dis_flag;

  limit = imin(6, start-4-pos);
  for (i = limit; i < 6; i++)
  {
    match[i] = -10.0;
  }

  /* Compare the 6-base region to AGGAGG */
  for (i = 0; i < limit; i++)
  {
    if (pos+i < 0)
    {
      continue;
    }
    if (i % 3 == 0)
    {
      if (is_a(seq, pos+i) == 1)
      {
        match[i] = 2.0;
      }
      else
      {
        match[i] = -3.0;
      }
    }
    else
    {
      if (is_g(seq, pos+i) == 1)
      {
        match[i] = 3.0;
      }
      else
      {
        match[i] = -2.0;
      }
    }
  }

  /* Find the maximally scoring motif */
  max_val = 0;
  for (i = limit; i >= 5; i--)
  {
    for (j = 0; j <= limit-i; j++)
    {
      cur_ctr = -2.0;
      mism = 0;
      for (k = j; k < j+i; k++)
      {
        cur_ctr += match[k];
        if (match[k] < 0.0)
        {
          mism++;
        }
        if (match[k] < 0.0 && (k <= j+1 || k >= j+i-2))
        {
          cur_ctr -= 10.0;
        }
      }
      if (mism != 1)
      {
        continue;
      }
      rdis = start - (pos+j+i);
      if (rdis < 5)
      {
        dis_flag = 1;
      }
      else if (rdis > 10 && rdis <= 12)
      {
        dis_flag = 2;
      }
      else if (rdis >= 13)
      {
        dis_flag = 3;
      }
      else
      {
        dis_flag = 0;
      }
      if (rdis > 15 || cur_ctr < 6.0)
      {
        continue;
      }

      /* Single-Mismatch RBS Motifs */
      if (cur_ctr < 6.0)
      {
        cur_val = 0;
      }
      else if (cur_ctr == 6.0 && dis_flag == 3)
      {
        cur_val = 2;
      }
      else if (cur_ctr == 7.0 && dis_flag == 3)
      {
        cur_val = 2;
      }
      else if (cur_ctr == 9.0 && dis_flag == 3)
      {
        cur_val = 3;
      }
      else if (cur_ctr == 6.0 && dis_flag == 2)
      {
        cur_val = 4;
      }
      else if (cur_ctr == 6.0 && dis_flag == 1)
      {
        cur_val = 5;
      }
      else if (cur_ctr == 6.0 && dis_flag == 0)
      {
        cur_val = 9;
      }
      else if (cur_ctr == 7.0 && dis_flag == 2)
      {
        cur_val = 7;
      }
      else if (cur_ctr == 7.0 && dis_flag == 1)
      {
        cur_val = 8;
      }
      else if (cur_ctr == 7.0 && dis_flag == 0)
      {
        cur_val = 14;
      }
      else if (cur_ctr == 9.0 && dis_flag == 2)
      {
        cur_val = 17;
      }
      else if (cur_ctr == 9.0 && dis_flag == 1)
      {
        cur_val = 18;
      }
      else if (cur_ctr == 9.0 && dis_flag == 0)
      {
        cur_val = 19;
      }

      if (rwt[cur_val] < rwt[max_val])
      {
        continue;
      }
      if (rwt[cur_val] == rwt[max_val] && cur_val < max_val)
      {
        continue;
      }
      max_val = cur_val;
    }
  }

  return max_val;
}

/* Zero out the sequence bitstrings for reuse */
void zero_sequence(unsigned char *seq, unsigned char *rseq,
                   unsigned char *useq, int slen)
{
  memset(seq, 0, (slen/4 + 1) * sizeof(unsigned char));
  memset(rseq, 0, (slen/4 + 1) * sizeof(unsigned char));
  memset(useq, 0, (slen/8 + 1) * sizeof(unsigned char));
}

/* Returns the minimum of two numbers */
int imin(int x, int y)
{
  if (x < y)
  {
    return x;
  }
  return y;
}
