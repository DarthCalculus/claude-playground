/*
 * Targeted search for puzzles with many blocks and holes.
 * Focuses on 5-6 blocks with 2-4 holes, no walls.
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
#define HT_SIZE (1<<24)
#define HT_MASK (HT_SIZE-1)
static uint64_t htk[HT_SIZE];static uint8_t htu[HT_SIZE];
static void htc(void){memset(htu,0,sizeof(htu));}
static inline uint64_t h64(uint64_t x){x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;x^=(x>>31);return x;}
static inline int hti(uint64_t k){uint64_t h=h64(k)&HT_MASK;for(int i=0;i<128;i++){uint64_t idx=(h+i)&HT_MASK;if(!htu[idx]){htu[idx]=1;htk[idx]=k;return 1;}if(htk[idx]==k)return 0;}return 0;}
#define QSZ (1<<24)
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

static uint64_t rng_s;
static inline uint64_t rnx(void){rng_s^=rng_s<<13;rng_s^=rng_s>>7;rng_s^=rng_s<<17;return rng_s;}
static inline int ri(int n){return(int)(rnx()%n);}

void ppz(const Puzzle*pz,int sc){
    char g[4][4];memset(g,'.',16);
    for(int i=0;i<16;i++)if(pz->walls&(1<<i))g[row_(i)][col_(i)]='#';
    for(int i=0;i<pz->num_holes;i++)g[row_(pz->hole_pos[i])][col_(pz->hole_pos[i])]='O';
    g[row_(pz->exit_pos)][col_(pz->exit_pos)]='E';g[row_(pz->player_start)][col_(pz->player_start)]='@';
    for(int i=0;i<pz->num_blocks;i++)g[row_(pz->block_pos[i])][col_(pz->block_pos[i])]='B';
    for(int r=0;r<4;r++){for(int c=0;c<4;c++){if(c)putchar(' ');putchar(g[r][c]);}putchar('\n');}
    const char*dn[]={"U","R","D","L"};
    printf("Exit:(%d,%d) Player:(%d,%d)\n",row_(pz->exit_pos),col_(pz->exit_pos),row_(pz->player_start),col_(pz->player_start));
    for(int i=0;i<pz->num_blocks;i++){printf("B%d@(%d,%d):",i,row_(pz->block_pos[i]),col_(pz->block_pos[i]));for(int d=0;d<4;d++)if(pz->block_pushable[i]&(1<<d))printf("%s",dn[d]);printf(" ");}
    printf("\nHoles:");for(int i=0;i<pz->num_holes;i++)printf("(%d,%d)",row_(pz->hole_pos[i]),col_(pz->hole_pos[i]));
    printf(" Score:%d\n\n",sc);
}

void mut(Puzzle*pz){
    uint16_t occ=pz->walls|(1<<pz->exit_pos)|(1<<pz->player_start);
    for(int i=0;i<pz->num_blocks;i++)occ|=(1<<pz->block_pos[i]);
    for(int i=0;i<pz->num_holes;i++)occ|=(1<<pz->hole_pos[i]);
    int fr[16],nf=0;for(int i=0;i<16;i++)if(!(occ&(1<<i)))fr[nf++]=i;
    switch(ri(12)){
    case 0:if(__builtin_popcount(pz->walls)>0&&nf>0){int w[16],nw=0;for(int i=0;i<16;i++)if(pz->walls&(1<<i))w[nw++]=i;int wi=w[ri(nw)];pz->walls&=~(1<<wi);pz->walls|=(1<<fr[ri(nf)]);}break;
    case 1:if(nf>0&&__builtin_popcount(pz->walls)<6)pz->walls|=(1<<fr[ri(nf)]);break;
    case 2:if(__builtin_popcount(pz->walls)>0){int w[16],nw=0;for(int i=0;i<16;i++)if(pz->walls&(1<<i))w[nw++]=i;pz->walls&=~(1<<w[ri(nw)]);}break;
    case 3:if(nf>0)pz->exit_pos=fr[ri(nf)];break;
    case 4:if(nf>0)pz->player_start=fr[ri(nf)];break;
    case 5:if(pz->num_blocks>0){int bf[16],nb=0;for(int i=0;i<nf;i++)if(fr[i]!=pz->exit_pos)bf[nb++]=fr[i];if(nb>0)pz->block_pos[ri(pz->num_blocks)]=bf[ri(nb)];}break;
    case 6:if(pz->num_blocks>0)pz->block_pushable[ri(pz->num_blocks)]=1+ri(15);break;
    case 7:if(pz->num_blocks<MAX_BLOCKS){int bf[16],nb=0;for(int i=0;i<nf;i++)if(fr[i]!=pz->exit_pos)bf[nb++]=fr[i];if(nb>0){pz->block_pos[pz->num_blocks]=bf[ri(nb)];pz->block_pushable[pz->num_blocks]=1+ri(15);pz->num_blocks++;}}break;
    case 8:if(pz->num_blocks>0){int bi=ri(pz->num_blocks);pz->block_pos[bi]=pz->block_pos[pz->num_blocks-1];pz->block_pushable[bi]=pz->block_pushable[pz->num_blocks-1];pz->num_blocks--;}break;
    case 9:if(pz->num_holes<MAX_HOLES&&nf>0){pz->hole_pos[pz->num_holes]=fr[ri(nf)];pz->num_holes++;}break;
    case 10:if(pz->num_holes>0){int hi=ri(pz->num_holes);pz->hole_pos[hi]=pz->hole_pos[pz->num_holes-1];pz->num_holes--;}break;
    case 11:if(pz->num_holes>0&&nf>0)pz->hole_pos[ri(pz->num_holes)]=fr[ri(nf)];break;
    }
}

int main(int argc,char**argv){
    int tl=300;if(argc>1)tl=atoi(argv[1]);
    rng_s=time(NULL)*6364136223846793005ULL+9876543210ULL;
    time_t st=time(NULL);
    int best=28;long long ev=0;

    Puzzle gbest;
    gbest.walls=0;gbest.exit_pos=pos(3,3);gbest.player_start=pos(2,2);
    gbest.num_blocks=5;
    gbest.block_pos[0]=pos(1,3);gbest.block_pushable[0]=1;
    gbest.block_pos[1]=pos(1,1);gbest.block_pushable[1]=14;
    gbest.block_pos[2]=pos(2,3);gbest.block_pushable[2]=7;
    gbest.block_pos[3]=pos(1,2);gbest.block_pushable[3]=1;
    gbest.block_pos[4]=pos(0,2);gbest.block_pushable[4]=15;
    gbest.num_holes=2;gbest.hole_pos[0]=pos(3,1);gbest.hole_pos[1]=pos(3,2);

    printf("Seed: 28\n\n");

    /* Focused random search: 5-6 blocks, 2-4 holes, 0-2 walls */
    typedef struct{int s;Puzzle p;} Seed;
    Seed seeds[500]; int ns=0;
    seeds[ns].s=28;seeds[ns].p=gbest;ns++;

    while(time(NULL)-st<tl){
        Puzzle pz;
        int mode=ri(10);

        if(mode<4&&ns>0){
            /* Mutate from a good seed */
            int si=ri(ns<20?ns:20);
            pz=seeds[si].p;
            int nm=1+ri(4);
            for(int m=0;m<nm;m++)mut(&pz);
        }else if(mode<6){
            /* Mutate from global best */
            pz=gbest;
            int nm=2+ri(5);
            for(int m=0;m<nm;m++)mut(&pz);
        }else{
            /* Targeted random: many blocks, some holes */
            int pm[16];for(int i=0;i<16;i++)pm[i]=i;
            for(int i=15;i>0;i--){int j=ri(i+1);int t=pm[i];pm[i]=pm[j];pm[j]=t;}

            int nw=ri(3); /* 0-2 walls */
            int idx=0;
            pz.walls=0;
            for(int i=0;i<nw;i++,idx++)pz.walls|=(1<<pm[idx]);
            pz.exit_pos=pm[idx++];pz.player_start=pm[idx++];

            int nb=4+ri(3); /* 4-6 blocks */
            if(nb>16-idx)nb=16-idx;
            pz.num_blocks=nb;
            for(int i=0;i<nb;i++){pz.block_pos[i]=pm[idx++];pz.block_pushable[i]=1+ri(15);}

            int nh=1+ri(4); /* 1-4 holes */
            if(nh>16-idx)nh=16-idx;
            if(nh>MAX_HOLES)nh=MAX_HOLES;
            pz.num_holes=nh;
            for(int i=0;i<nh;i++)pz.hole_pos[i]=pm[idx++];
        }

        int s=solve(&pz);ev++;
        if(s>best){
            best=s;gbest=pz;
            printf("[%lds] New best: %d (ev=%lld)\n",time(NULL)-st,s,ev);
            ppz(&pz,s);fflush(stdout);
        }
        if(s>0&&s>=best-5&&ns<500){seeds[ns].s=s;seeds[ns].p=pz;ns++;}
        if(ns>=500){
            /* Keep top 200 */
            for(int i=0;i<ns-1;i++)for(int j=i+1;j<ns;j++)if(seeds[j].s>seeds[i].s){Seed t=seeds[i];seeds[i]=seeds[j];seeds[j]=t;}
            ns=200;
        }
        if(ev%200000==0){printf("[%lds] ev=%lld best=%d seeds=%d\n",time(NULL)-st,ev,best,ns);fflush(stdout);}
    }

    printf("\n==================================================\n");
    printf("FINAL: %d steps (%lld evals)\n",best,ev);
    printf("==================================================\n");
    ppz(&gbest,best);
    return 0;
}
