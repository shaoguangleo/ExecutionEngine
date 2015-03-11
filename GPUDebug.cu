template <int T>
__global__ void debugMark() {
};

#ifdef __DEBUG
void debugMark(int t) {
   switch(t) {
      case 0: debugMark<0><<<1,1>>>(); break;
      case 1: debugMark<1><<<1,1>>>(); break;
      case 2: debugMark<2><<<1,1>>>(); break;
      case 3: debugMark<3><<<1,1>>>(); break;
      case 4: debugMark<4><<<1,1>>>(); break;
      case 5: debugMark<5><<<1,1>>>(); break;
      case 6: debugMark<6><<<1,1>>>(); break;
      case 7: debugMark<7><<<1,1>>>(); break;
      case 8: debugMark<8><<<1,1>>>(); break;
      case 9: debugMark<9><<<1,1>>>(); break;
      default: 
         break;
   };

}
#else
void debugMark(int t) {if (0==t) debugMark<0><<<1,1>>>();};
#endif
