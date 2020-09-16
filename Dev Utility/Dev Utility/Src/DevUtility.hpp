/**
 * @file Contains the includes and definitions necessary for the Add-On to
 *       function.
 */

#if !defined (__DEVUTILITY_HPP__)
#define __DEVUTILITY_HPP__

#ifdef _WIN32
	#pragma warning (push, 3)
	#include	<Win32Interface.hpp>
	#pragma warning (pop)

	#ifndef WINDOWS
		#define WINDOWS
	#endif
#endif

#ifdef macintosh
	#include <CoreServices/CoreServices.h>
#endif

#ifndef ACExtension
	#define	ACExtension
#endif

#ifdef WINDOWS
	#pragma warning (disable: 4068)
#endif

#include "ACAPinc.h"
#include "Location.hpp"
#include "DGModule.hpp"
#include "UC.h"
#include "DG.h"
#include "APICommon.h"
#include "BM.hpp"
#include "IOBase.hpp"
#include "Location.hpp"
#include "Folder.hpp"
#include "File.hpp"
#include "FileSystem.hpp"
#define _USE_MATH_DEFINES
#include <math.h>

#ifdef WINDOWS
	#pragma warning (default: 4068)
#endif

#pragma warning (disable: 4239)
#pragma warning (disable: 4127)

#define TRUE	1
#define FALSE	0


#endif //__DEVUTILITY_HPP__


// 변수 선언
enum	idxItems;			// 열거형: 다이얼로그 항목 인덱스

// 함수 선언
std::string		format_string (const std::string fmt, ...);		// std::string 변수 값에 formatted string을 입력 받음
GSErrCode		placeCoordinateOnTarget (void);					// 1번 메뉴: 좌표 객체를 배치하는 통합 루틴
GSErrCode		showGeometricalDataOnMorph (void);				// 2번 메뉴: 모프 객체의 기하 데이터를 좌표 객체로 보여주는 통합 루틴
static short	DGCALLBACK	placerHandler (short message, short dialogID, short item, DGUserData userData, DGMessageData msgData);		// DG 콜백 함수
GSErrCode		placeCoordinateLabel (double xPos, double yPos, double zPos, bool bComment, std::string comment, short layerInd);		// 좌표 라벨을 배치함
void			createCoordinateLibPart (void);																							// 좌표 라벨 라이브러리 파트가 없으면 만듦
double	degreeToRad (double degree);							// degree 각도를 radian 각도로 변환
double	RadToDegree (double rad);								// radian 각도를 degree 각도로 변환

// 다이얼로그 항목 인덱스
enum	idxItems {
	ICON_LAYER					= 3,
	USERCONTROL_LAYER			= 4,
	CHECKBOX_LOCALORIG			= 5,
	BUTTON_SELECT_LOCALORIG		= 6,
	CENTERTEXT_TARGET_OBJECT	= 7,
	LEFTTEXT_FONTSIZE			= 8,
	EDITCONTROL_FONTSIZE		= 9
};
