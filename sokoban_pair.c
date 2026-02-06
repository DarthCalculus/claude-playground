/*
 * Exhaustive pair-mutation search from known 28-step puzzle.
 * Try all pairs of changes (pushable sides, positions, etc.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define GRID_SIZE 4
#define MAX_BLOCKS 6
#define MAX_HOLES 4
static const int DR[4]={-1,0,1,0}, DC[4]={0,1,0,-1};
typedef struct { uint16_t walls; int exit_pos,player_start,num_blocks; int block_pos[MAX_BLOCKS]; uint8_t block_pushable[MAX_BLOCKS]; int num_holes,hole_pos[MAX_HOLES]; } Puzzle;
static inline int pos(int r,int c){return r*4+c;}
static inline int row_(int p){return p/4;}
static inline int col_(int p){return p%4;}
static inline int inb(int r,int c){return r>=0&&r<4&&c>=0&&c<4;}
static inline uint64_t pack(int pl,int*bp,int nb,int hm){uint64_t s=pl;int sh=4;for(int i=0;i<nb;i++){s|=((uint64_t)bp[i]<<sh);sh+=5;}s|=((uint64_t)hm<<sh);return s;}
#define HT_SIZE (1<<23)
#define HT_MASK (HT_SIZE-1)
static uint64_t htk[HT_SIZE];static uint8_t htu[HT_SIZE];
static void htc(void){memset(htu,0,sizeof(htu));}
static inline uint64_t h64(uint64_t x){x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;x^=(x>>31);return x;}
static inline int hti(uint64_t k){uint64_t h=h64(k)&HT_MASK;for(int i=0;i<64;i++){uint64_t idx=(h+i)&HT_MASK;if(!htu[idx]){htu[idx]=1;htk[idx]=k;return 1;}if(htk[idx]==k)return 0;}return 0;}
#define QSZ (1<<23)
static uint64_t qs[QSZ];static int qd[QSZ];

int solve(const Puzzle*pz){
    htc();int ib[MAX_BLOCKS];for(int i=0;i<pz->num_blocks;i++)ib[i]=pz->block_pos[i];
    int ihm=(1<<pz->num_holes)-1;uint64_t st=pack(pz->player_start,ib,pz->num_blocks,ihm);hti(st);
    int qh=0,qt=0;qs[qt]=st;qd[qt]=0;qt++;
    while(qh<qt){uint64_t s=qs[qh];int dist=qd[qh];qh++;int pl=s&0xF,sh=4,bp[MAX_BLOCKS];
    for(int i=0;i<pz->num_blocks;i++){bp[i]=(s>>sh)&0x1F;sh+=5;}int hm=(s>>sh)&0xF,pr=row_(pl),pc=col_(pl);
    for(int d=0;d<4;d++){int nr=pr+DR[d],nc=pc+DC[d];if(!inb(nr,nc))continue;int np=pos(nr,nc);
    if(pz->walls&(1<<np))continue;int uh=0;for(int h=0;h<pz->num_holes;h++)if(pz->hole_pos[h]==np&&(hm&(1<<h))){uh=1;break;}if(uh)continue;
    int bi=-1;for(int b=0;b<pz->num_blocks;b++)if(bp[b]==np){bi=b;break;}
    if(bi>=0){if(!(pz->block_pushable[bi]&(1<<d)))continue;int bnr=nr+DR[d],bnc=nc+DC[d];if(!inb(bnr,bnc))continue;int bnp=pos(bnr,bnc);
    if(pz->walls&(1<<bnp))continue;int bl=0;for(int b=0;b<pz->num_blocks;b++)if(b!=bi&&bp[b]==bnp){bl=1;break;}if(bl)continue;
    int nb[MAX_BLOCKS];memcpy(nb,bp,sizeof(int)*pz->num_blocks);int nhm=hm,ih=0;
    for(int h=0;h<pz->num_holes;h++)if(pz->hole_pos[h]==bnp&&(hm&(1<<h))){ih=1;nb[bi]=16;nhm&=~(1<<h);break;}
    if(!ih)nb[bi]=bnp;if(np==pz->exit_pos)return dist+1;
    uint64_t ns=pack(np,nb,pz->num_blocks,nhm);if(hti(ns)){if(qt>=QSZ)return-2;qs[qt]=ns;qd[qt]=dist+1;qt++;}}
    else{if(np==pz->exit_pos)return dist+1;uint64_t ns=pack(np,bp,pz->num_blocks,hm);if(hti(ns)){if(qt>=QSZ)return-2;qs[qt]=ns;qd[qt]=dist+1;qt++;}}}}
    return-1;
}

void ppz(const Puzzle*pz,int sc){
    char g[4][4];memset(g,'.',16);
    for(int i=0;i<16;i++)if(pz->walls&(1<<i))g[row_(i)][col_(i)]='#';
    for(int i=0;i<pz->num_holes;i++)g[row_(pz->hole_pos[i])][col_(pz->hole_pos[i])]='O';
    g[row_(pz->exit_pos)][col_(pz->exit_pos)]='E';g[row_(pz->player_start)][col_(pz->player_start)]='@';
    for(int i=0;i<pz->num_blocks;i++)g[row_(pz->block_pos[i])][col_(pz->block_pos[i])]='B';
    for(int r=0;r<4;r++){for(int c=0;c<4;c++){if(c)putchar(' ');putchar(g[r][c]);}putchar('\n');}
    const char*dn[]={"U","R","D","L"};
    for(int i=0;i<pz->num_blocks;i++){printf("B%d@(%d,%d):",i,row_(pz->block_pos[i]),col_(pz->block_pos[i]));for(int d=0;d<4;d++)if(pz->block_pushable[i]&(1<<d))printf("%s",dn[d]);printf(" ");}
    printf("\nHoles:");for(int i=0;i<pz->num_holes;i++)printf("(%d,%d)",row_(pz->hole_pos[i]),col_(pz->hole_pos[i]));
    printf(" Score:%d\n",sc);
}

int main(){
    Puzzle base;
    base.walls=0;base.exit_pos=pos(3,3);base.player_start=pos(2,2);
    base.num_blocks=5;
    base.block_pos[0]=pos(1,3);base.block_pushable[0]=1;
    base.block_pos[1]=pos(1,1);base.block_pushable[1]=14;
    base.block_pos[2]=pos(2,3);base.block_pushable[2]=7;
    base.block_pos[3]=pos(1,2);base.block_pushable[3]=1;
    base.block_pos[4]=pos(0,2);base.block_pushable[4]=15;
    base.num_holes=2;base.hole_pos[0]=pos(3,1);base.hole_pos[1]=pos(3,2);

    printf("Base: %d\n",solve(&base));
    int best=28;long long ev=0;

    /* Try all pairs of pushable side changes */
    printf("=== Pairs of pushable changes ===\n");fflush(stdout);
    for(int b1=0;b1<5;b1++)for(int ps1=1;ps1<=15;ps1++){
        for(int b2=b1;b2<5;b2++)for(int ps2=1;ps2<=15;ps2++){
            if(b1==b2&&ps2<=ps1)continue;
            Puzzle t=base;t.block_pushable[b1]=ps1;t.block_pushable[b2]=ps2;
            int s=solve(&t);ev++;
            if(s>best){best=s;printf("New best %d: b%d=%d b%d=%d\n",s,b1,ps1,b2,ps2);ppz(&t,s);fflush(stdout);}
        }
    }
    printf("After pushable pairs: best=%d (ev=%lld)\n\n",best,ev);fflush(stdout);

    /* Try all pairs: one pushable change + one position change */
    printf("=== Pushable + position pairs ===\n");fflush(stdout);
    uint16_t occ_base=base.walls|(1<<base.exit_pos)|(1<<base.player_start);
    for(int i=0;i<base.num_holes;i++)occ_base|=(1<<base.hole_pos[i]);

    for(int b1=0;b1<5;b1++)for(int ps1=1;ps1<=15;ps1++){
        /* + move another block */
        for(int b2=0;b2<5;b2++){
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)if(i!=b2)occ|=(1<<base.block_pos[i]);
            for(int p=0;p<16;p++){
                if((occ&(1<<p))||p==base.exit_pos)continue;
                Puzzle t=base;t.block_pushable[b1]=ps1;t.block_pos[b2]=p;
                int s=solve(&t);ev++;
                if(s>best){best=s;printf("New best %d: ps[%d]=%d pos[%d]=%d\n",s,b1,ps1,b2,p);ppz(&t,s);fflush(stdout);}
            }
        }
        /* + move player */
        {
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)occ|=(1<<base.block_pos[i]);
            for(int p=0;p<16;p++){
                if((occ&(1<<p))||p==base.player_start)continue;
                Puzzle t=base;t.block_pushable[b1]=ps1;t.player_start=p;
                int s=solve(&t);ev++;
                if(s>best){best=s;printf("New best %d: ps[%d]=%d player=%d\n",s,b1,ps1,p);ppz(&t,s);fflush(stdout);}
            }
        }
        /* + move exit */
        {
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)occ|=(1<<base.block_pos[i]);
            for(int p=0;p<16;p++){
                if((occ&(1<<p))||p==base.exit_pos)continue;
                Puzzle t=base;t.block_pushable[b1]=ps1;t.exit_pos=p;
                int s=solve(&t);ev++;
                if(s>best){best=s;printf("New best %d: ps[%d]=%d exit=%d\n",s,b1,ps1,p);ppz(&t,s);fflush(stdout);}
            }
        }
        /* + move hole */
        for(int h=0;h<2;h++){
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)occ|=(1<<base.block_pos[i]);
            uint16_t hocc=0;for(int i=0;i<2;i++)if(i!=h)hocc|=(1<<base.hole_pos[i]);
            for(int p=0;p<16;p++){
                if(((occ|hocc)&(1<<p))||p==base.hole_pos[h])continue;
                Puzzle t=base;t.block_pushable[b1]=ps1;t.hole_pos[h]=p;
                int s=solve(&t);ev++;
                if(s>best){best=s;printf("New best %d: ps[%d]=%d hole[%d]=%d\n",s,b1,ps1,h,p);ppz(&t,s);fflush(stdout);}
            }
        }
        /* + add wall */
        {
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)occ|=(1<<base.block_pos[i]);
            for(int p=0;p<16;p++){
                if(occ&(1<<p))continue;
                Puzzle t=base;t.block_pushable[b1]=ps1;t.walls|=(1<<p);
                int s=solve(&t);ev++;
                if(s>best){best=s;printf("New best %d: ps[%d]=%d wall@%d\n",s,b1,ps1,p);ppz(&t,s);fflush(stdout);}
            }
        }
    }
    printf("After mixed pairs: best=%d (ev=%lld)\n\n",best,ev);fflush(stdout);

    /* Try all pairs of block position changes */
    printf("=== Position pairs ===\n");fflush(stdout);
    for(int b1=0;b1<5;b1++){
        for(int b2=b1+1;b2<5;b2++){
            uint16_t occ=occ_base;
            for(int i=0;i<5;i++)if(i!=b1&&i!=b2)occ|=(1<<base.block_pos[i]);
            for(int p1=0;p1<16;p1++){
                if((occ&(1<<p1))||p1==base.exit_pos)continue;
                for(int p2=0;p2<16;p2++){
                    if(p2==p1||(occ&(1<<p2))||p2==base.exit_pos)continue;
                    Puzzle t=base;t.block_pos[b1]=p1;t.block_pos[b2]=p2;
                    int s=solve(&t);ev++;
                    if(s>best){best=s;printf("New best %d: pos[%d]=%d pos[%d]=%d\n",s,b1,p1,b2,p2);ppz(&t,s);fflush(stdout);}
                }
            }
        }
    }
    printf("After position pairs: best=%d (ev=%lld)\n\n",best,ev);fflush(stdout);

    printf("==================================================\n");
    printf("FINAL: %d steps (%lld evaluations)\n",best,ev);
    printf("==================================================\n");
    return 0;
}
