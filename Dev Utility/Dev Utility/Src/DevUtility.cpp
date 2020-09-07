/**
 * @file Contains the functions required by ArchiCAD.
 */

#include <cstdio>
#include <cstdlib>
#include "DevUtility.hpp"
#include "Definitions.hpp"
#include "StringConversion.hpp"

#define MDID_DEVELOPER_ID	829517673
#define MDID_LOCAL_ID		3897128265

static bool		bLocalOrigin;	// true이면 로컬 원점, false이면 글로벌 원점
static double	szFont;			// 글꼴 크기
static short	layerInd;		// 레이어 인덱스
// ... 로컬 원점 좌표 구조체: Coords
// ... vertices 리스트: Coords 배열
// ... 객체 리스트: API_Element

/**
 * Dependency definitions. Function name is fixed.
 *
 * @param envir [in] ArchiCAD environment values.
 * @return The Add-On loading type.
 */
API_AddonType	__ACENV_CALL	CheckEnvironment (API_EnvirParams* envir)
{
	ACAPI_Resource_GetLocStr (envir->addOnInfo.name, 32000, 1);
	ACAPI_Resource_GetLocStr (envir->addOnInfo.description, 32000, 2);

	return APIAddon_Normal;
}		// CheckEnvironment ()

/**
 * Interface definitions. Function name is fixed.
 *
 * @return ArchiCAD error code.
 */
GSErrCode	__ACENV_CALL	RegisterInterface (void)
{
	//
	// Register a menu
	//
	GSErrCode err = ACAPI_Register_Menu (32500, 32520, MenuCode_UserDef, MenuFlag_Default);

	return err;
}		// RegisterInterface ()

/**
 * Menu command handler function. Function name is NOT fixed. There can be
 * more than one of these functions. Please check the API Development Kit
 * documentation for more information.
 *
 * @param params [in] Parameters of the menu command.
 * @return ArchiCAD error code.
 */
GSErrCode __ACENV_CALL	CommandHandler (const API_MenuParams *menuParams)
{
	GSErrCode	err = NoError;

	switch (menuParams->menuItemRef.menuResID) {
		case 32500:
			switch (menuParams->menuItemRef.itemIndex) {
				case 1:		err = placeCoordinateOnTarget ();		break;	// place "Coordinate" objects on vertices (target: Polyline, Morph)
			}
	}

	return err;
}		// CommandHandler ()

/**
 * Called after the Add-On has been loaded into memory. Function name is fixed.
 *
 * @return ArchiCAD error code.
 */
GSErrCode __ACENV_CALL	Initialize (void)
{
	GSErrCode err = ACAPI_Install_MenuHandler (32500, CommandHandler);

	// register special help location if needed
	// for Graphisoft's add-ons, this is the Help folder beside the installed ArchiCAD
	IO::Location		helpLoc;
	API_SpecFolderID	specID = API_HelpFolderID;

	ACAPI_Environment (APIEnv_GetSpecFolderID, &specID, (void*) &helpLoc);
	DG::RegisterAdditionalHelpLocation (MDID_DEVELOPER_ID, MDID_LOCAL_ID, &helpLoc);

	return err;
}		// Initialize ()

/**
 * Called when the Add-On is going to be unloaded. Function name is fixed.
 *
 * @return ArchiCAD error code.
 */
GSErrCode __ACENV_CALL	FreeData (void)

{
	DG::UnregisterAdditionalHelpLocation (MDID_DEVELOPER_ID, MDID_LOCAL_ID);

	return NoError;
}		// FreeData ()

// 1번 메뉴: 좌표 객체를 배치하는 통합 루틴
GSErrCode	placeCoordinateOnTarget (void)
{
	GSErrCode err = NoError;
	short	itemInd = DG_OK;

	while (itemInd != DG_CANCEL) {
		itemInd = DGModalDialog (ACAPI_GetOwnResModule (), 32500, ACAPI_GetOwnResModule (), placerHandler, NULL);
	}

	return err;
}

static short	DGCALLBACK	placerHandler (short message, short dialogID, short item, DGUserData /* userData */, DGMessageData /* msgData */)
{
	short		result;
	API_UCCallbackType	ucb;

	switch (message) {
		case DG_MSG_INIT:
			
			// 다이얼로그 타이틀
			DGSetDialogTitle (dialogID, "객체 좌표 배치");

			// 버튼
			DGSetItemText (dialogID, DG_OK, "배치");
			DGSetItemText (dialogID, DG_CANCEL, "종료");

			// 유저 컨트롤 초기화
			BNZeroMemory (&ucb, sizeof (ucb));
			ucb.dialogID = dialogID;
			ucb.type	 = APIUserControlType_Layer;
			ucb.itemID	 = USERCONTROL_LAYER;
			ACAPI_Interface (APIIo_SetUserControlCallbackID, &ucb, NULL);
			DGSetItemValLong (dialogID, USERCONTROL_LAYER, 1);

			// 로컬원점 - 체크박스, 버튼
			DGSetItemText (dialogID, CHECKBOX_LOCALORIG, "로컬원점");
			DGSetItemText (dialogID, BUTTON_SELECT_LOCALORIG, "로컬원점 선택");

			// 객체선택 - 버튼
			DGSetItemText (dialogID, BUTTON_SELECT_OBJECT, "객체 선택");

			// 글꼴크기 - 라벨
			DGSetItemText (dialogID, LEFTTEXT_FONTSIZE, "글꼴 크기");
			DGSetItemValDouble (dialogID, EDITCONTROL_FONTSIZE, 0.100);		// 기본값 100

			break;

		case DG_MSG_CHANGE:
			break;

		case DG_MSG_CLICK:
			switch (item) {
				case DG_OK:			// 배치 버튼
					// ... 만약 '객체 선택'하지 않으면 배치되지 않음
					break;
				
					// 라이브러리가 없으면 직접 생성하기 ***

					// 정점에다가 좌표 라이브러리 객체 배치

				case DG_CANCEL:		// 종료 버튼
					item = DG_CANCEL;
					break;
				
				case BUTTON_SELECT_LOCALORIG:	// 로컬원점 선택 버튼
					// ... 2D/3D/단면/입면/실내입면 상에서 점 클릭 가능
					break;
				
				case BUTTON_SELECT_OBJECT:		// 객체 선택 버튼
					// 선택한 요소 가져오기
					// ... ACAPI_Selection_Get

					// ... 꼭지점을 가져올 객체 선택
						// 폴리라인인 경우
						// 모프인 경우

					break;
			}
		case DG_MSG_CLOSE:
			switch (item) {
				case DG_CLOSEBOX:
					break;
			}
	}

	result = item;

	return	result;
}

/*
// 좌표 라벨을 배치함
GSErrCode	placeCoordinateLabel (double xPos, double yPos, double zPos, std::string comment, short layerInd, short floorInd)
{
	GSErrCode	err = NoError;

	API_Element			element;
	API_ElementMemo		memo;
	API_LibPart			libPart;

	const	GS::uchar_t* gsmName = NULL;
	double	aParam;
	double	bParam;
	Int32	addParNum;

	// 라이브러리 이름 선택
	gsmName = L("좌표 19.gsm");

	// 객체 로드
	BNZeroMemory (&libPart, sizeof (libPart));
	GS::ucscpy (libPart.file_UName, gsmName);
	err = ACAPI_LibPart_Search (&libPart, false);
	if (err != NoError)
		return err;
	if (libPart.location != NULL)
		delete libPart.location;

	ACAPI_LibPart_Get (&libPart);

	BNZeroMemory (&element, sizeof (API_Element));
	BNZeroMemory (&memo, sizeof (API_ElementMemo));

	element.header.typeID = API_ObjectID;
	element.header.guid = GSGuid2APIGuid (GS::Guid (libPart.ownUnID));

	ACAPI_Element_GetDefaults (&element, &memo);
	ACAPI_LibPart_GetParams (libPart.index, &aParam, &bParam, &addParNum, &memo.params);

	// 라이브러리의 파라미터 값 입력
	element.object.libInd = libPart.index;
	element.object.pos.x = xPos;
	element.object.pos.y = yPos;
	element.object.level = zPos;
	element.object.xRatio = aParam;
	element.object.yRatio = bParam;
	element.header.layer = layerInd;
	element.header.floorInd = floorInd;
	memo.params [0][15].value.real = TRUE;
	GS::ucscpy (memo.params [0][16].value.uStr, GS::UniString (comment.c_str ()).ToUStr ().Get ());

	// 객체 배치
	ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);

	return	err;
}
*/