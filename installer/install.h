/*
 * resource set indices for test
 *
 * created by ORCS 2.18
 */

/*
 * Number of Strings:        18
 * Number of Bitblks:        0
 * Number of Iconblks:       0
 * Number of Color Iconblks: 0
 * Number of Color Icons:    0
 * Number of Tedinfos:       0
 * Number of Free Strings:   5
 * Number of Free Images:    0
 * Number of Objects:        19
 * Number of Trees:          1
 * Number of Userblks:       0
 * Number of Images:         0
 * Total file size:          1024
 */

#undef RSC_NAME
#ifndef __ALCYON__
#define RSC_NAME "test"
#endif
#undef RSC_ID
#ifdef test
#define RSC_ID test
#else
#define RSC_ID 0
#endif

#ifndef RSC_STATIC_FILE
# define RSC_STATIC_FILE 0
#endif
#if !RSC_STATIC_FILE
#define NUM_STRINGS 18
#define NUM_FRSTR 5
#define NUM_UD 0
#define NUM_IMAGES 0
#define NUM_BB 0
#define NUM_FRIMG 0
#define NUM_IB 0
#define NUM_CIB 0
#define NUM_TI 0
#define NUM_OBS 19
#define NUM_TREE 1
#endif



#define MNU_MAIN                           0 /* menu */
#define MNU_ABOUT                          7 /* STRING in tree MNU_MAIN */
#define MNU_LOCATE                        16 /* STRING in tree MNU_MAIN */
#define MNU_INSTALL                       17 /* STRING in tree MNU_MAIN */
#define MNU_QUIT                          18 /* STRING in tree MNU_MAIN */

#define AL_ABOUT                           0 /* Alert string */

#define AL_INSTALL                         1 /* Alert string */

#define ST_SELECT                          2 /* Free string */

#define AL_NOEMUTOS                        3 /* Alert string */

#define AL_INSTALLERR                      4 /* Alert string */




#ifdef __STDC__
#ifndef _WORD
#  ifdef WORD
#    define _WORD WORD
#  else
#    define _WORD short
#  endif
#endif
extern _WORD test_rsc_load(_WORD wchar, _WORD hchar);
extern _WORD test_rsc_gaddr(_WORD type, _WORD idx, void *gaddr);
extern _WORD test_rsc_free(void);
#endif
