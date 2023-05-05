#ifndef P2P_COMMON_H
#define P2P_COMMON_H

#include "ticks.h"
#include <cstdarg>
#include <cstring>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#if defined(__ANDROID__)
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG   "flycast"
#endif
#define ZGS_LOG(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s:%d] " fmt, __FILE_NAME__, __LINE__, ##args)
#else
#define ZGS_LOG(fmt, args...) printf("%s:%d] " fmt "\n", __FILE_NAME__, __LINE__, ##args)
#endif

/********************** GAMEID **************************/
#define MAXCOMROOM 200
#define MAXSPROOM 20

// #define USE_NETVS
// #define USE_GGPO_ROLLBACK
#define USE_DELAY

/********************** GAMEID [END]**************************/

/****************** Notify CMD [START] ******************/
#define CMD_MATCHRKEY 2
#define CMD_SYNCSTATEKEY 3
#define CMD_PINGKEY 13
#define CMD_ONLINE_START_GAME 40          ///< 游戏开始的标志
#define CMD_NETWORK_STATE 41              ///< 网络状态：中断、断开、恢复
#define CONNETCTION_INTTERUPT 0           ///< 网络状态：中断
#define CONNETCTION_DISCONNECTED 1        ///< 网络状态：断开
#define CONNETCTION_RESUMED 2              ///< 网络状态：恢复

// For kHostFlag
#define SINGLEPLAYER 0
#define NETVS_P1 1
#define NETVS_P2 2
#define PLAYERWATCH 4
extern int kHostFlag;
extern bool kNetVSGame;

// For kNetRunFrame
#define VSFRAME_SYNCING 1
#define P1WIN 0
#define P2WIN 1
#define DRAW 2

extern int P2PCMode;
extern pthread_mutex_t gEmuStateMutex;

// For Game Info
extern unsigned char gOKAIGameType;

// watch need
extern char mInternetIP[];
extern char pInternetIP[];
extern unsigned short pP2PUDPPort;

extern void UpdateOption(int name, int value, const char *arg1,
                         const char *arg2, int arg3, int arg4);
extern void NETVSRunOneFrame(int bDraw, uint16_t aP1Keys, uint16_t aP2Keys, uint16_t aP3Keys, uint16_t aP4Keys);

#endif
