#include "../src/dsp.h"
#include <stdio.h>
#include <vector>
#include <ctime>
using namespace dyn;
static double runSine(Engine& e, double freq, double ampDb, double fs, int ch, double& grOut, double& limOut, int secs=1) {
    int N = (int)(fs*secs); std::vector<float> buf(N*ch);
    double a = dbToLin(ampDb);
    for (int i=0;i<N;i++) for(int c=0;c<ch;c++) buf[i*ch+c]=(float)(a*sin(2*PI*freq*i/fs));
    Engine::BlockMeters m; unsigned sp=0; double pk=0; grOut=0; limOut=0;
    for (int off=0; off<N; off+=480){ unsigned n = (N-off)<480?(N-off):480; e.process(&buf[off*ch], &buf[off*ch], n, m, nullptr,0,sp); grOut=m.gr[0]>m.gr[1]?m.gr[0]:m.gr[1]; limOut=m.grLim;}
    for (int i=N/2;i<N;i++) { double v=fabs(buf[i*ch]); if(v>pk) pk=v; }
    return linToDb(pk);
}
int main(){
    double fs=48000; double gr, lim;
    DynParams p; DynDefaultParams(p);
    // 1) tout neutre : EQ plat, comp desactive, limiteur off -> gain unite
    p.compEnabled=0; p.limEnabled=0;
    Engine e; e.applyParams(p); e.prepare(fs,2);
    printf("neutre 1kHz -6dB -> %.3f dB (attendu -6)\n", runSine(e,1000,-6,fs,2,gr,lim));
    // 2) peak EQ +6 dB a 1kHz
    p.eq[5].gain=6; e.applyParams(p); e.reset();
    printf("EQ +6@1k -> %.3f dB (attendu 0)\n", runSine(e,1000,-6,fs,2,gr,lim));
    printf("reponse theorique @1k = %.3f, @100Hz=%.3f\n", eqResponseDb(p,1000,fs), eqResponseDb(p,100,fs));
    p.eq[5].gain=0;
    // 3) compresseur : T=-20 R=4 knee 0, sinus -8 dB -> sortie -20 + 12/4 = -17
    p.compEnabled=1; p.comp[0]={1,-20,4,1,50,0,0,0}; p.scHpf=0;
    e.applyParams(p); e.reset();
    printf("comp T-20 R4, in -8 -> %.3f dB (attendu ~-17), GR %.2f\n", runSine(e,1000,-8,fs,2,gr,lim,2), gr);
    // 4) multibande neutre (ratio 1) doit etre ~transparent en amplitude
    p.compMultiband=1; for(int b=1;b<4;b++){p.comp[b]={1,-20,1,5,100,0,0,0};}
    e.applyParams(p); e.reset();
    for (double f: {50.0,150.0,1000.0,3000.0,10000.0}) printf("multibande ratio1 %.0fHz -> %.3f dB\n", f, runSine(e,f,-6,fs,2,gr,lim));
    // 5) limiteur : +6 dB in, plafond -1
    p.compEnabled=0; p.compMultiband=0; p.limEnabled=1; p.limCeiling=-1;
    e.applyParams(p); e.reset();
    printf("limiteur in +6 plafond -1 -> %.3f dB, GR %.2f\n", runSine(e,1000,6,fs,2,gr,lim), lim);
    // 6) bypass
    p.bypass=1; e.applyParams(p);
    printf("bypass in +6 -> %.3f dB (attendu 6)\n", runSine(e,1000,6,fs,2,gr,lim));
    // 7) coupe-bas 48 dB/oct a 100 Hz, verif -3dB a fc et pente
    DynDefaultParams(p); p.compEnabled=0; p.limEnabled=0; p.eq[0]={1,EQ_LOWCUT,100,0,0.707f,4};
    printf("lowcut48 @100=%.2f @50=%.2f @25=%.2f\n", eqResponseDb(p,100,fs), eqResponseDb(p,50,fs), eqResponseDb(p,25,fs));
    // 8) perf : 10 bandes actives + multibande
    DynDefaultParams(p); for(int i=0;i<10;i++) p.eq[i].gain=3; p.compMultiband=1;
    e.applyParams(p); e.prepare(fs,2);
    std::vector<float> b(480*2, 0.1f); Engine::BlockMeters m; unsigned sp=0;
    clock_t t=clock(); for(int i=0;i<100*60;i++) e.process(b.data(),b.data(),480,m,nullptr,0,sp);
    printf("60 s d'audio stereo traites en %.3f s CPU\n", (double)(clock()-t)/CLOCKS_PER_SEC);
}
