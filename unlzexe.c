/* unlzexe.c
* unlzexe ver 0.5 (PC-VAN UTJ44266 Kou )
*   UNLZEXE converts the compressed file by lzexe(ver.0.90,0.91) to the
*   UNcompressed executable one.
*
*   usage:  UNLZEXE packedfile[.EXE] [unpackedfile.EXE]
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define FAILURE 1
#define SUCCESS 0

typedef uint16_t WORD;
typedef uint8_t BYTE;
#define stricmp strcasecmp

int isjapan(void);
int japan_f;
#define	iskanji(c)	('\x81'<=(c)&&(c)<='\x9f' || '\xe0'<=(c)&&(c)<='\xfc')

char *tmpfname = "$tmpfil$.exe";
char *backup_ext = ".olz";
char ipath[FILENAME_MAX],
     opath[FILENAME_MAX],
     ofname[13];

int fnamechk(char*,char*,char*,int,char**);
int fnamechg(char*,char*,char*,int);
int rdhead(FILE *,int *);
int mkreltbl(FILE *,FILE *,int);
int unpack(FILE *,FILE *);
void wrhead(FILE *);
int reloc90();
int reloc91();

int main(int argc,char **argv){
    FILE *ifile,*ofile;
    int  ver,rename_sw=0;

    printf("UNLZEXE Ver. 0.5\n");
    japan_f=isjapan();
    if(argc!=3 && argc!=2){
        printf("usage: UNLZEXE packedfile [unpackedfile]\n");
        exit(EXIT_FAILURE);
    }
    if(argc==2)
        rename_sw=1;
    if(fnamechk(ipath,opath,ofname,argc,argv)!=SUCCESS) {
        exit(EXIT_FAILURE);
    }
    if((ifile=fopen(ipath,"rb"))==NULL){
        printf("'%s' :not found\n",ipath);
            exit(EXIT_FAILURE);
    }

    if(rdhead(ifile,&ver)!=SUCCESS){
        printf("'%s' is not LZEXE file.\n",ipath);
        fclose(ifile); exit(EXIT_FAILURE);
    }
    if((ofile=fopen(opath,"w+b"))==NULL){
        printf("can't open '%s'.\n",opath);
        fclose(ifile); exit(EXIT_FAILURE);
    }
    printf("file '%s' is compressed by LZEXE Ver. ",ipath);
    switch(ver){
    case 90: printf("0.90\n"); break;
    case 91: printf("0.91\n"); break;
    }
    if(mkreltbl(ifile,ofile,ver)!=SUCCESS) {
        fclose(ifile);
        fclose(ofile);
        remove(opath);
        exit(EXIT_FAILURE);
    }
    if(unpack(ifile,ofile)!=SUCCESS) {
        fclose(ifile);
        fclose(ofile);
        remove(opath);
        exit(EXIT_FAILURE);
    }
    fclose(ifile);
    wrhead(ofile);
    fclose(ofile);

    if(fnamechg(ipath,opath,ofname,rename_sw)!=SUCCESS){
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}



void parsepath(char *pathname, int *fname, int *ext);

/* file name check */
int fnamechk(char *ipath,char *opath, char *ofname,
              int argc,char **argv) {
    int idx_name,idx_ext;

    strcpy(ipath,argv[1]);
    parsepath(ipath,&idx_name,&idx_ext);
    if (! ipath[idx_ext]) strcpy(ipath+idx_ext,".exe");
    if(! stricmp(ipath+idx_name,tmpfname)){
        printf("'%s':bad filename.\n",ipath);
        return(FAILURE);
    }
    if(argc==2)
        strcpy(opath,ipath);
    else
        strcpy(opath,argv[2]);
    parsepath(opath,&idx_name,&idx_ext);
    if (! opath[idx_ext]) strcpy(opath+idx_ext,".exe");
    if (!stricmp(opath+idx_ext,backup_ext)){
        printf("'%s':bad filename.\n",opath);
        return(FAILURE);
    }
    strncpy(ofname,opath+idx_name,12);
    strcpy(opath+idx_name,tmpfname);
    return(SUCCESS);
}


int fnamechg(char *ipath,char *opath,char *ofname,int rename_sw) {
    int idx_name,idx_ext;
    char tpath[FILENAME_MAX];

    if(rename_sw) {
        strcpy(tpath,ipath);
        parsepath(tpath,&idx_name,&idx_ext);
        strcpy(tpath+idx_ext,backup_ext);
        remove(tpath);
        if(rename(ipath,tpath)){
            printf("can't make '%s'.\n", tpath);
            remove(opath);
            return(FAILURE);
        }
	printf("'%s' is renamed to '%s'.\n",ipath,tpath);
    }
    strcpy(tpath,opath);
    parsepath(tpath,&idx_name,&idx_ext);
    strcpy(tpath+idx_name,ofname);
    remove(tpath);
    if(rename(opath,tpath)){
        if(rename_sw) {
            strcpy(tpath,ipath);
            parsepath(tpath,&idx_name,&idx_ext);
            strcpy(tpath+idx_ext,backup_ext);
            rename(tpath,ipath);
        }
        printf("can't make '%s'.  unpacked file '%s' is remained.\n",
                 tpath, tmpfname);

        return(FAILURE);
    }
    printf("unpacked file '%s' is generated.\n",tpath);
    return(SUCCESS);
}

int isjapan() {
/*
    union REGS r;
    struct SREGS rs;
    BYTE buf[34];

    segread(&rs);
    rs.ds=rs.ss;  r.x.dx=(WORD)buf;
    r.h.al=0x3800;
    intdosx(&r,&r,&rs);
    return(!strcmp(buf+2,"\\"));
*/
    return 0;
}

void parsepath(char *pathname, int *fname, int *ext) {
    /* use  int japan_f */
    char c;
    int i;

    *fname=0; *ext=0;
    for(i=0;c=pathname[i];i++) {
        if(japan_f && iskanji(c))
            i++;
        else
            switch(c) {
            case ':' :
            case '\\':  *fname=i+1; break;
            case '.' :  *ext=i; break;
            default  :  ;
            }
    }
    if(*ext<=*fname) *ext=i;
}
/*-------------------------------------------*/
static WORD ihead[0x10],ohead[0x10],inf[8];
static WORD allocsize;
static long loadsize;

/* EXE header test (is it LZEXE file?) */
int rdhead(FILE *ifile ,int *ver){
    if(fread(ihead,sizeof ihead[0],0x10,ifile)!=0x10)
        return FAILURE;
    memcpy(ohead,ihead,sizeof ihead[0] * 0x10);
    if(ihead[0]!=0x5a4d || ihead[4]!=2 || ihead[0x0d]!=0)
        return FAILURE;
    if(ihead[0x0c]==0x1c && memcmp(&ihead[0x0e],"LZ09",4)==0){
        *ver=90; return SUCCESS ;
    }
    if(ihead[0x0c]==0x1c && memcmp(&ihead[0x0e],"LZ91",4)==0){
        *ver=91; return SUCCESS ;
    }
    return FAILURE;
}

/* make relocation table */
int mkreltbl(FILE *ifile,FILE *ofile,int ver) {
    long fpos;
    int i;

    allocsize=((ihead[1]+16-1)>>4) + ((ihead[2]-1)<<5) - ihead[4] + ihead[5];
    fpos=(long)(ihead[0x0b]+ihead[4])<<4;		/* goto CS:0000 */
    fseek(ifile,fpos,SEEK_SET);
    fread(inf, sizeof inf[0], 0x08, ifile);
    ohead[0x0a]=inf[0];		/* IP */
    ohead[0x0b]=inf[1];		/* CS */
    ohead[0x08]=inf[2];		/* SP */
    ohead[0x07]=inf[3];		/* SS */
    /* inf[4]:size of compressed load module (PARAGRAPH)*/
    /* inf[5]:increase of load module size (PARAGRAPH)*/
    /* inf[6]:size of decompressor with  compressed relocation table (BYTE) */
    /* inf[7]:check sum of decompresser with compressd relocation table(Ver.0.90) */
    ohead[0x0c]=0x1c;		/* start position of relocation table */
    fseek(ofile,0x1cL,SEEK_SET);
    switch(ver){
    case 90: i=reloc90(ifile,ofile,fpos);
             break;
    case 91: i=reloc91(ifile,ofile,fpos);
             break;
    default: i=FAILURE; break;
    }
    if(i!=SUCCESS){
        printf("error at relocation table.\n");
        return (FAILURE);
    }
    fpos=ftell(ofile);
    i=fpos & 0x1ff;
    if(i) i=0x200-i;
    ohead[4]=(fpos+i)>>4;

    for( ; i>0; i--)
        putc(0, ofile);
    return(SUCCESS);
}
/* for LZEXE ver 0.90 */
int reloc90(FILE *ifile,FILE *ofile,long fpos) {
    unsigned int c;
    WORD rel_count=0;
    WORD rel_seg,rel_off;

    fseek(ifile,fpos+0x19d,SEEK_SET);
    				/* 0x19d=compressed relocation table address */
    rel_seg=0;
    do{
        if(feof(ifile) || ferror(ifile) || ferror(ofile)) return(FAILURE);
        fread (&c, 2, 1, ifile);
        for(;c>0;c--) {
            fread (&rel_off, 2, 1, ifile);
            fwrite (&rel_off, 2, 1, ofile);
            fwrite (&rel_seg, 2, 1, ofile);
            rel_count++;
        }
        rel_seg += 0x1000;
    } while(rel_seg!=(0xf000+0x1000));
    ohead[3]=rel_count;
    return(SUCCESS);
}
/* for LZEXE ver 0.91*/
int reloc91(FILE *ifile,FILE *ofile,long fpos) {
    WORD span;
    WORD rel_count=0;
    WORD rel_seg,rel_off;

    fseek(ifile,fpos+0x158,SEEK_SET);
    				/* 0x158=compressed relocation table address */
    rel_off=0; rel_seg=0;
    for(;;) {
        if (feof(ifile) || ferror(ifile) || ferror(ofile)) return(FAILURE);
        if((span=(BYTE)getc(ifile))==0) {
            fread(&span, 2, 1, ifile);
            if(span==0){
                rel_seg += 0x0fff;
                continue;
            } else if(span==1){
                break;
            }
        }
        rel_off += span;
        rel_seg += (rel_off & ~0x0f)>>4;
        rel_off &= 0x0f;
        fwrite(&rel_off, 2, 1, ofile);
        fwrite(&rel_seg, 2, 1, ofile);
        rel_count++;
    }
    ohead[3]=rel_count;
    return(SUCCESS);
}

/*---------------------*/
typedef struct {
        FILE  *fp;
        WORD  buf;
        BYTE  count;
    } bitstream;

void initbits(bitstream *,FILE *);
int getbit(bitstream *);

/*---------------------*/
/* decompressor routine */
int unpack(FILE *ifile,FILE *ofile){
    int len;
    WORD span;
    long fpos;
    bitstream bits;
    static BYTE data[0x4500], *p=data;

    fpos=(long)(ihead[0x0b]-inf[4]+ihead[4])<<4;
    fseek(ifile,fpos,SEEK_SET);
    fpos=(long)ohead[4]<<4;
    fseek(ofile,fpos,SEEK_SET);
    initbits(&bits,ifile);
    printf(" unpacking. ");
    for(;;){
        if(ferror(ifile)) {printf("\nread error\n"); return(FAILURE); }
        if(ferror(ofile)) {printf("\nwrite error\n"); return(FAILURE); }
        if(p-data>0x4000){
            fwrite(data,sizeof data[0],0x2000,ofile);
            p-=0x2000;
            memcpy(data,data+0x2000,p-data);
            putchar('.');
        }
        if(getbit(&bits)) {
            *p++=(BYTE)getc(ifile);
            continue;
        }
        if(!getbit(&bits)) {
            len=getbit(&bits)<<1;
            len |= getbit(&bits);
            len += 2;
            span=(BYTE)getc(ifile) | 0xff00;
        } else {
            span=(BYTE)getc(ifile);
            len=(BYTE)getc(ifile);
            span |= ((len & ~0x07)<<5) | 0xe000;
            len = (len & 0x07)+2;
            if (len==2) {
                len=(BYTE)getc(ifile);

                if(len==0)
                    break;    /* end mark of compreesed load module */

                if(len==1)
                    continue; /* segment change */
                else
                    len++;
            }
        }
        for( ;len>0;len--,p++)
            *p=*(p+(int16_t)span);
    }
    if(p!=data)
        fwrite(data,sizeof data[0],p-data,ofile);
    loadsize=ftell(ofile)-fpos;
    printf("end\n");
    return(SUCCESS);
}

/* write EXE header*/
void wrhead(FILE *ofile) {
    if(ihead[6]!=0) {
        ohead[5]=allocsize-((loadsize+16-1)>>4);
        if(ihead[6]!=0xffff)
            ohead[6]-=(ihead[5]-ohead[5]);
    }
    ohead[1]=(loadsize+(ohead[4]<<4)) & 0x1ff;
    ohead[2]=(loadsize+(ohead[4]<<4)+0x1ff) >>9;
    fseek(ofile,0L,SEEK_SET);
    fwrite(ohead,sizeof ohead[0],0x0e,ofile);
}

/*-------------------------------------------*/

/* get compress information bit by bit */
void initbits(bitstream *p,FILE *filep){
    p->fp=filep;
    p->count=0x10;
    fread(&p->buf, 2, 1, p->fp);
    /* printf("%04x ",p->buf); */
}

int getbit(bitstream *p) {
    int b;
    b = p->buf & 1;
    if(--p->count == 0){
        fread(&p->buf, 2, 1, p->fp);
        /* printf("%04x ",p->buf); */
        p->count= 0x10;
    }else
        p->buf >>= 1;

    return b;
}
