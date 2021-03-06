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

static bool					bLocalOrigin;			// true이면 로컬 원점, false이면 글로벌 원점
static double				szFont;					// 글꼴 크기
static double				angle;					// 회전 각도
static short				layerInd;				// 레이어 인덱스
static API_GetPointType		localOriginPointInfo;	// 로컬 원점

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
				case 2:		err = showGeometricalDataOnMorph ();	break;	// place "Coordinate" objects on Morph's vertices
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

// std::string 변수 값에 formatted string을 입력 받음
std::string	format_string (const std::string fmt, ...)
{
	int			size = ((int)fmt.size ()) * 2;
	va_list		ap;
	std::string	buffer;

	while (1) {
		buffer.resize(size);
		va_start (ap, fmt);
		int n = vsnprintf ((char*)buffer.data (), size, fmt.c_str (), ap);
		va_end (ap);

		if (n > -1 && n < size) {
			buffer.resize (n);
			return buffer;
		}

		if (n > -1)
			size = n + 1;
		else
			size *= 2;
	}

	return buffer;
}

// 1번 메뉴: 좌표 객체를 배치하는 통합 루틴
GSErrCode		placeCoordinateOnTarget (void)
{
	GSErrCode err = NoError;
	short	itemInd = DG_OK;
	std::string		tempString;

	// Selection Manager 관련 변수
	API_SelectionInfo		selectionInfo;
	API_Neig				**selNeigs;
	long					nSel;
	API_Element				elem;
	API_ElementMemo			memo;
	API_ElemInfo3D			info3D;

	// 모프 3D 구성요소 가져오기
	API_Component3D			component;
	API_Tranmat				tm;
	Int32					nVert, nEdge, nPgon;
	Int32					elemIdx, bodyIdx;
	API_Coord3D				trCoord;
	GS::Array<API_Coord3D>&	coords = GS::Array<API_Coord3D> ();
	long					nNodes;
	API_Coord3D				point3D;

	// 로컬 원점 정보
	const char*			prompt_localorigin = "로컬 원점을 클릭하십시오.";
	localOriginPointInfo.pos.x = 0;
	localOriginPointInfo.pos.y = 0;
	localOriginPointInfo.pos.z = 0;
	layerInd = 1;
	bLocalOrigin = false;
	szFont = 0.100;
	angle = 0.0;

	// 다이얼로그 호출
	do {
		itemInd = DGModalDialog (ACAPI_GetOwnResModule (), 32500, ACAPI_GetOwnResModule (), placerHandler, NULL);

		// 로컬원점 선택 버튼
		if (itemInd == BUTTON_SELECT_LOCALORIG) {
			BNZeroMemory (&localOriginPointInfo, sizeof (API_GetPointType));
			CHTruncate (prompt_localorigin, localOriginPointInfo.prompt, sizeof (localOriginPointInfo.prompt));
			localOriginPointInfo.changeFilter = false;
			localOriginPointInfo.changePlane  = false;
			err = ACAPI_Interface (APIIo_GetPointID, &localOriginPointInfo, NULL);
		}
	} while (itemInd == BUTTON_SELECT_LOCALORIG);

	if (itemInd != DG_OK)	return -1;	// 실행하지 않고 종료

	// 선택한 요소 가져오기
	err = ACAPI_Selection_Get (&selectionInfo, &selNeigs, true);
	BMKillHandle ((GSHandle *) &selectionInfo.marquee.coords);
	if (err == APIERR_NOSEL) {
		ACAPI_WriteReport ("아무 것도 선택하지 않았습니다.", true);
		return err;
	}
	if (err != NoError) {
		BMKillHandle ((GSHandle *) &selNeigs);
		return err;
	}
	BNZeroMemory (&elem, sizeof (API_Element));
	BNZeroMemory (&memo, sizeof (API_ElementMemo));

	// 좌표 객체 배치
	err = ACAPI_CallUndoableCommand ("좌표 객체 배치", [&] () -> GSErrCode {
		if (selectionInfo.typeID != API_SelEmpty) {
			nSel = BMGetHandleSize ((GSHandle) selNeigs) / sizeof (API_Neig);
			for (int xx = 0 ; xx < nSel && err == NoError ; ++xx) {
				elem.header.typeID = Neig_To_ElemID ((*selNeigs)[xx].neigID);
				elem.header.guid = (*selNeigs)[xx].guid;

				ACAPI_Element_Get (&elem);
				ACAPI_Element_GetMemo (elem.header.guid, &memo);

				// 폴리라인일 경우,
				if (elem.header.typeID == API_PolyLineID) {
					for (int yy = 1 ; yy <= elem.polyLine.poly.nCoords ; ++yy) {
						err = placeCoordinateLabel (memo.coords [0][yy].x, memo.coords [0][yy].y, 0, false, "", layerInd);
					}
				}

				// 선일 경우,
				if (elem.header.typeID == API_LineID) {
					err = placeCoordinateLabel (elem.line.begC.x, elem.line.begC.y, 0, false, tempString, layerInd);
					err = placeCoordinateLabel (elem.line.endC.x, elem.line.endC.y, 0, false, tempString, layerInd);
				}

				// 호일 경우,
				if (elem.header.typeID == API_ArcID) {
					tempString = format_string ("R: %.4f, begAng: %.2f, endAng: %.2f", elem.arc.r, RadToDegree (elem.arc.begAng), RadToDegree (elem.arc.endAng));
					err = placeCoordinateLabel (elem.arc.origC.x, elem.arc.origC.y, 0, true, tempString, layerInd);
				}

				// 원일 경우,
				if (elem.header.typeID == API_CircleID) {
					tempString = format_string ("R: %.4f", elem.circle.r);
					err = placeCoordinateLabel (elem.circle.origC.x, elem.circle.origC.y, 0, true, tempString, layerInd);
				}

				// 채우기일 경우,
				if (elem.header.typeID == API_HatchID) {
					for (int yy = 1 ; yy <= elem.hatch.poly.nCoords ; ++yy) {
						err = placeCoordinateLabel (memo.coords [0][yy].x, memo.coords [0][yy].y, 0, false, "", layerInd);
					}
				}

				// 모프일 경우,
				if (elem.header.typeID == API_MorphID) {
					ACAPI_Element_Get3DInfo (elem.header, &info3D);

					// 모프의 3D 바디를 가져옴
					BNZeroMemory (&component, sizeof (API_Component3D));
					component.header.typeID = API_BodyID;
					component.header.index = info3D.fbody;
					err = ACAPI_3D_GetComponent (&component);

					// 모프의 3D 모델을 가져오지 못하면 종료
					if (err != NoError) {
						ACAPI_WriteReport ("모프의 3D 모델을 가져오지 못했습니다.", true);
						return err;
					}

					nVert = component.body.nVert;
					nEdge = component.body.nEdge;
					nPgon = component.body.nPgon;
					tm = component.body.tranmat;
					elemIdx = component.body.head.elemIndex - 1;
					bodyIdx = component.body.head.bodyIndex - 1;
	
					// 정점 좌표를 임의 순서대로 저장함
					for (xx = 1 ; xx <= nVert ; ++xx) {
						component.header.typeID	= API_VertID;
						component.header.index	= xx;
						err = ACAPI_3D_GetComponent (&component);
						if (err == NoError) {
							trCoord.x = tm.tmx[0]*component.vert.x + tm.tmx[1]*component.vert.y + tm.tmx[2]*component.vert.z + tm.tmx[3];
							trCoord.y = tm.tmx[4]*component.vert.x + tm.tmx[5]*component.vert.y + tm.tmx[6]*component.vert.z + tm.tmx[7];
							trCoord.z = tm.tmx[8]*component.vert.x + tm.tmx[9]*component.vert.y + tm.tmx[10]*component.vert.z + tm.tmx[11];
							coords.Push (trCoord);
						}
					}
					nNodes = coords.GetSize ();

					tempString = format_string ("%s", "MIN 값");
					err = placeCoordinateLabel (info3D.bounds.xMin, info3D.bounds.yMin, info3D.bounds.zMin, true, tempString, layerInd);
					tempString = format_string ("%s", "MAX 값");
					err = placeCoordinateLabel (info3D.bounds.xMax, info3D.bounds.yMax, info3D.bounds.zMax, true, tempString, layerInd);

					for (xx = 1 ; xx <= nNodes ; ++xx) {
						point3D = coords.Pop ();

						tempString = format_string ("%d번", xx);
						err = placeCoordinateLabel (point3D.x, point3D.y, point3D.z, true, tempString, layerInd);
					}
				}

				// 보일 경우,
				if (elem.header.typeID == API_BeamID) {
					tempString = format_string ("시작");
					err = placeCoordinateLabel (elem.beam.begC.x, elem.beam.begC.y, elem.beam.level, true, tempString, layerInd);
					tempString = format_string ("끝");
					err = placeCoordinateLabel (elem.beam.endC.x, elem.beam.endC.y, elem.beam.level, true, tempString, layerInd);
				}
			}
		}

		return err;
	});
	ACAPI_DisposeElemMemoHdls (&memo);
	BMKillHandle ((GSHandle *) &selNeigs);

	return err;
}

// 2번 메뉴: 모프 객체의 기하 데이터를 좌표 객체로 보여주는 통합 루틴
GSErrCode		showGeometricalDataOnMorph (void)
{
	GSErrCode err = NoError;
	short	itemInd = DG_OK;
	std::string		tempString;

	// Selection Manager 관련 변수
	API_SelectionInfo		selectionInfo;
	API_Neig				**selNeigs;
	long					nSel;
	API_Element				elem;
	API_ElementMemo			memo;
	API_ElemInfo3D			info3D;

	// 모프 3D 구성요소 가져오기
	API_Component3D			component;
	API_Tranmat				tm;
	Int32					nVert, nEdge, nPgon;
	Int32					elemIdx, bodyIdx;
	API_Coord3D				trCoord;
	GS::Array<API_Coord3D>&	coords = GS::Array<API_Coord3D> ();
	long					nNodes;
	API_Coord3D				point3D;

	// 로컬 원점 정보
	const char*			prompt_localorigin = "로컬 원점을 클릭하십시오.";
	localOriginPointInfo.pos.x = 0;
	localOriginPointInfo.pos.y = 0;
	localOriginPointInfo.pos.z = 0;
	layerInd = 1;
	bLocalOrigin = false;
	szFont = 0.100;
	angle = 0.0;

	// 다이얼로그 호출
	do {
		itemInd = DGModalDialog (ACAPI_GetOwnResModule (), 32500, ACAPI_GetOwnResModule (), placerHandler, NULL);

		// 로컬원점 선택 버튼
		if (itemInd == BUTTON_SELECT_LOCALORIG) {
			BNZeroMemory (&localOriginPointInfo, sizeof (API_GetPointType));
			CHTruncate (prompt_localorigin, localOriginPointInfo.prompt, sizeof (localOriginPointInfo.prompt));
			localOriginPointInfo.changeFilter = false;
			localOriginPointInfo.changePlane  = false;
			err = ACAPI_Interface (APIIo_GetPointID, &localOriginPointInfo, NULL);
		}
	} while (itemInd == BUTTON_SELECT_LOCALORIG);

	if (itemInd != DG_OK)	return -1;	// 실행하지 않고 종료

	// 선택한 요소 가져오기
	err = ACAPI_Selection_Get (&selectionInfo, &selNeigs, true);
	BMKillHandle ((GSHandle *) &selectionInfo.marquee.coords);
	if (err == APIERR_NOSEL) {
		ACAPI_WriteReport ("아무 것도 선택하지 않았습니다.", true);
		return err;
	}
	if (err != NoError) {
		BMKillHandle ((GSHandle *) &selNeigs);
		return err;
	}
	BNZeroMemory (&elem, sizeof (API_Element));
	BNZeroMemory (&memo, sizeof (API_ElementMemo));

	// 좌표 객체 배치
	err = ACAPI_CallUndoableCommand ("좌표 객체 배치", [&] () -> GSErrCode {
		if (selectionInfo.typeID != API_SelEmpty) {
			nSel = BMGetHandleSize ((GSHandle) selNeigs) / sizeof (API_Neig);
			for (int xx = 0 ; xx < nSel && err == NoError ; ++xx) {
				elem.header.typeID = Neig_To_ElemID ((*selNeigs)[xx].neigID);
				elem.header.guid = (*selNeigs)[xx].guid;

				ACAPI_Element_Get (&elem);
				ACAPI_Element_GetMemo (elem.header.guid, &memo);

				// 모프
				if (elem.header.typeID == API_MorphID) {
					ACAPI_Element_Get3DInfo (elem.header, &info3D);

					// 모프의 3D 바디를 가져옴
					BNZeroMemory (&component, sizeof (API_Component3D));
					component.header.typeID = API_BodyID;
					component.header.index = info3D.fbody;
					err = ACAPI_3D_GetComponent (&component);

					// 모프의 3D 모델을 가져오지 못하면 종료
					if (err != NoError) {
						ACAPI_WriteReport ("모프의 3D 모델을 가져오지 못했습니다.", true);
						return err;
					}

					nVert = component.body.nVert;
					nEdge = component.body.nEdge;
					nPgon = component.body.nPgon;
					tm = component.body.tranmat;
					elemIdx = component.body.head.elemIndex - 1;
					bodyIdx = component.body.head.bodyIndex - 1;
	
					// 정점 좌표를 임의 순서대로 저장함
					for (xx = 1 ; xx <= nVert ; ++xx) {
						component.header.typeID	= API_VertID;
						component.header.index	= xx;
						err = ACAPI_3D_GetComponent (&component);
						if (err == NoError) {
							trCoord.x = tm.tmx[0]*component.vert.x + tm.tmx[1]*component.vert.y + tm.tmx[2]*component.vert.z + tm.tmx[3];
							trCoord.y = tm.tmx[4]*component.vert.x + tm.tmx[5]*component.vert.y + tm.tmx[6]*component.vert.z + tm.tmx[7];
							trCoord.z = tm.tmx[8]*component.vert.x + tm.tmx[9]*component.vert.y + tm.tmx[10]*component.vert.z + tm.tmx[11];
							coords.Push (trCoord);
						}
					}
					nNodes = coords.GetSize ();

					tempString = format_string ("%s", "MIN 값");
					err = placeCoordinateLabel (info3D.bounds.xMin, info3D.bounds.yMin, info3D.bounds.zMin, true, tempString, layerInd);
					tempString = format_string ("%s", "MAX 값");
					err = placeCoordinateLabel (info3D.bounds.xMax, info3D.bounds.yMax, info3D.bounds.zMax, true, tempString, layerInd);

					for (xx = 1 ; xx <= nNodes ; ++xx) {
						point3D = coords.Pop ();

						tempString = format_string ("%d번", xx);
						err = placeCoordinateLabel (point3D.x, point3D.y, point3D.z, true, tempString, layerInd);
					}

					tempString = format_string ("%s", "MIN 값");
					err = placeCoordinateLabel (info3D.bounds.xMin, info3D.bounds.yMin, info3D.bounds.zMin, true, tempString, layerInd);
					tempString = format_string ("%s", "MAX 값");
					err = placeCoordinateLabel (info3D.bounds.xMax, info3D.bounds.yMax, info3D.bounds.zMax, true, tempString, layerInd);
					tempString = format_string ("%s", "TMX 값 [3,7,11]");
					err = placeCoordinateLabel (elem.morph.tranmat.tmx [3], elem.morph.tranmat.tmx [7], elem.morph.tranmat.tmx [11], true, tempString, layerInd);
				}
			}
		}

		return err;
	});
	ACAPI_DisposeElemMemoHdls (&memo);
	BMKillHandle ((GSHandle *) &selNeigs);

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
			DGSetItemValLong (dialogID, USERCONTROL_LAYER, layerInd);

			// 로컬원점 - 체크박스, 버튼
			DGSetItemText (dialogID, CHECKBOX_LOCALORIG, "로컬원점");
			DGSetItemText (dialogID, BUTTON_SELECT_LOCALORIG, "로컬원점 선택");
			DGSetItemValLong (dialogID, CHECKBOX_LOCALORIG, bLocalOrigin);

			// 글꼴크기 - 라벨, Edit 컨트롤
			DGSetItemText (dialogID, LEFTTEXT_FONTSIZE, "글꼴 크기");
			DGSetItemValDouble (dialogID, EDITCONTROL_FONTSIZE, szFont);

			// 회전각도 - 라벨, Edit 컨트롤
			DGSetItemText (dialogID, LEFTTEXT_ANGLE, "회전 각도");
			DGSetItemValDouble (dialogID, EDITCONTROL_ANGLE, angle);

			break;
		
		case DG_MSG_CLICK:
			switch (item) {
				case BUTTON_SELECT_LOCALORIG:	// 로컬원점 선택 버튼
					// 2D/3D/단면/입면/실내입면 상에서 점 클릭 가능
					// 호출자에서 처리함
					layerInd = (short)DGGetItemValLong (dialogID, USERCONTROL_LAYER);
					if (DGGetItemValLong (dialogID, CHECKBOX_LOCALORIG) == TRUE)
						bLocalOrigin = true;
					else
						bLocalOrigin = false;
					szFont = DGGetItemValDouble (dialogID, EDITCONTROL_FONTSIZE);
					angle = DGGetItemValDouble (dialogID, EDITCONTROL_ANGLE);
					break;
				
				case DG_OK:			// 배치 버튼
					// 레이어 번호
					layerInd = (short)DGGetItemValLong (dialogID, USERCONTROL_LAYER);

					// 로컬 원점인지 아닌지 여부
					if (DGGetItemValLong (dialogID, CHECKBOX_LOCALORIG) == TRUE)
						bLocalOrigin = true;
					else
						bLocalOrigin = false;

					// 글꼴 크기
					szFont = DGGetItemValDouble (dialogID, EDITCONTROL_FONTSIZE);

					// 회전 각도
					angle = DGGetItemValDouble (dialogID, EDITCONTROL_ANGLE);

					break;
					
				case DG_CANCEL:		// 종료 버튼
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

// 좌표 라벨을 배치함
GSErrCode	placeCoordinateLabel (double xPos, double yPos, double zPos, bool bComment, std::string comment, short layerInd)
{
	GSErrCode	err = NoError;

	API_Element			element;
	API_ElementMemo		memo;
	API_LibPart			libPart;

	const	GS::uchar_t* gsmName = NULL;
	double	aParam;
	double	bParam;
	Int32	addParNum;

	API_StoryInfo	storyInfo;
	double			minusLevel;

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

	// 작업 층 높이 반영
	BNZeroMemory (&storyInfo, sizeof (API_StoryInfo));
	minusLevel = 0.0;
	ACAPI_Environment (APIEnv_GetStorySettingsID, &storyInfo);
	for (int xx = 0 ; xx < (storyInfo.lastStory - storyInfo.firstStory) ; ++xx) {
		if (storyInfo.data [0][xx].index == element.header.floorInd) {
			minusLevel = storyInfo.data [0][xx].level;
			break;
		}
	}
	BMKillHandle ((GSHandle *) &storyInfo.data);

	// 라이브러리의 파라미터 값 입력
	element.object.libInd = libPart.index;
	element.object.pos.x = xPos;
	element.object.pos.y = yPos;
	element.object.level = zPos - minusLevel;
	element.object.xRatio = aParam;
	element.object.yRatio = bParam;
	element.object.angle = degreeToRad (angle);
	element.header.layer = layerInd;
	if (bLocalOrigin == true)
		memo.params [0][8].value.real = TRUE;	// 로컬 원점 활성화
	else
		memo.params [0][8].value.real = FALSE;	// 로컬 원점 비활성화
	memo.params [0][10].value.real = szFont;		// 글꼴 크기
	memo.params [0][15].value.real = bComment;		// 코멘트 On/Off
	GS::ucscpy (memo.params [0][16].value.uStr, GS::UniString (comment.c_str ()).ToUStr ().Get ());		// 코멘트 내용

	// 로컬 원점 좌표 입력 (Examples -> LibPart_Test)
	double** arrHdl = reinterpret_cast<double**>(memo.params [0][9].value.array);
	(*arrHdl)[0] = localOriginPointInfo.pos.x;
	(*arrHdl)[1] = localOriginPointInfo.pos.y;
	(*arrHdl)[2] = localOriginPointInfo.pos.z;

	// 객체 배치
	ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);

	return	err;
}

// degree 각도를 radian 각도로 변환
double	degreeToRad (double degree)
{
	return degree * M_PI / 180;
}

// radian 각도를 degree 각도로 변환
double	RadToDegree (double rad)
{
	return rad * 180 / M_PI;
}