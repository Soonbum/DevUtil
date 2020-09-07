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
struct	Coords;				// 좌표 정보

// 함수 선언
GSErrCode	placeCoordinateOnTarget (void);		// 1번 메뉴: 좌표 객체를 배치하는 통합 루틴
static short	DGCALLBACK	placerHandler (short message, short dialogID, short item, DGUserData userData, DGMessageData msgData);		// DG 콜백 함수

// 다이얼로그 항목 인덱스
enum	idxItems {
	ICON_LAYER					= 3,
	USERCONTROL_LAYER			= 4,
	CHECKBOX_LOCALORIG			= 5,
	BUTTON_SELECT_LOCALORIG		= 6,
	BUTTON_SELECT_OBJECT		= 7,
	CENTERTEXT_TARGET_OBJECT	= 8,
	LEFTTEXT_FONTSIZE			= 9,
	EDITCONTROL_FONTSIZE		= 10
};

// 구조체 선언
struct Coords {
	double	xPos;
	double	yPos;
	double	zPos;
};