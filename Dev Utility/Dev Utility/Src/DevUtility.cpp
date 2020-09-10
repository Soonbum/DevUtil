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

static bool					bLocalOrigin;			// true�̸� ���� ����, false�̸� �۷ι� ����
static double				szFont;					// �۲� ũ��
static short				layerInd;				// ���̾� �ε���
static API_GetPointType		localOriginPointInfo;	// ���� ����

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

// std::string ���� ���� formatted string�� �Է� ����
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

// 1�� �޴�: ��ǥ ��ü�� ��ġ�ϴ� ���� ��ƾ
GSErrCode		placeCoordinateOnTarget (void)
{
	GSErrCode err = NoError;
	short	itemInd = DG_OK;
	std::string		tempString;

	// Selection Manager ���� ����
	API_SelectionInfo		selectionInfo;
	API_Neig				**selNeigs;
	long					nSel;
	API_Element				elem;
	API_ElementMemo			memo;
	API_ElemInfo3D			info3D;

	// ���� ���� ����
	const char*			prompt_localorigin = "���� ������ Ŭ���Ͻʽÿ�.";
	localOriginPointInfo.pos.x = 0;
	localOriginPointInfo.pos.y = 0;
	localOriginPointInfo.pos.z = 0;
	layerInd = 1;
	bLocalOrigin = false;
	szFont = 0.100;

	// ���̾�α� ȣ��
	do {
		itemInd = DGModalDialog (ACAPI_GetOwnResModule (), 32500, ACAPI_GetOwnResModule (), placerHandler, NULL);

		// ���ÿ��� ���� ��ư
		if (itemInd == BUTTON_SELECT_LOCALORIG) {
			BNZeroMemory (&localOriginPointInfo, sizeof (API_GetPointType));
			CHTruncate (prompt_localorigin, localOriginPointInfo.prompt, sizeof (localOriginPointInfo.prompt));
			localOriginPointInfo.changeFilter = false;
			localOriginPointInfo.changePlane  = false;
			err = ACAPI_Interface (APIIo_GetPointID, &localOriginPointInfo, NULL);
		}
	} while (itemInd == BUTTON_SELECT_LOCALORIG);

	if (itemInd != DG_OK)	return -1;	// �������� �ʰ� ����

	// ������ ��� ��������
	err = ACAPI_Selection_Get (&selectionInfo, &selNeigs, true);
	BMKillHandle ((GSHandle *) &selectionInfo.marquee.coords);
	if (err == APIERR_NOSEL) {
		ACAPI_WriteReport ("�ƹ� �͵� �������� �ʾҽ��ϴ�.", true);
		return err;
	}
	if (err != NoError) {
		BMKillHandle ((GSHandle *) &selNeigs);
		return err;
	}
	BNZeroMemory (&elem, sizeof (API_Element));
	BNZeroMemory (&memo, sizeof (API_ElementMemo));

	// ��ǥ ��ü ��ġ
	if (selectionInfo.typeID != API_SelEmpty) {
		nSel = BMGetHandleSize ((GSHandle) selNeigs) / sizeof (API_Neig);
		for (int xx = 0 ; xx < nSel && err == NoError ; ++xx) {
			elem.header.typeID = Neig_To_ElemID ((*selNeigs)[xx].neigID);
			elem.header.guid = (*selNeigs)[xx].guid;

			ACAPI_Element_Get (&elem);
			ACAPI_Element_GetMemo (elem.header.guid, &memo);

			// ���������� ���,
			if (elem.header.typeID == API_PolyLineID) {
				err = ACAPI_CallUndoableCommand ("��ǥ ��ü ��ġ", [&] () -> GSErrCode {
					for (int yy = 1 ; yy <= elem.polyLine.poly.nCoords ; ++yy) {
						err = placeCoordinateLabel (memo.coords [0][yy].x, memo.coords [0][yy].y, 0, false, "", layerInd);
					}

					return err;
				});

			}

			// ������ ���,
			if (elem.header.typeID == API_MorphID) {
				ACAPI_Element_Get3DInfo (elem.header, &info3D);

				err = ACAPI_CallUndoableCommand ("��ǥ ��ü ��ġ", [&] () -> GSErrCode {
					tempString = format_string ("%s", "MIN ��");
					err = placeCoordinateLabel (info3D.bounds.xMin, info3D.bounds.yMin, info3D.bounds.zMin, true, tempString, layerInd);
					tempString = format_string ("%s", "MAX ��");
					err = placeCoordinateLabel (info3D.bounds.xMax, info3D.bounds.yMax, info3D.bounds.zMax, true, tempString, layerInd);

					return err;
				});
			}
		}
	}
	ACAPI_DisposeElemMemoHdls (&memo);
	BMKillHandle ((GSHandle *) &selNeigs);

	return err;
}

// 2�� �޴�: ���� ��ü�� ���� �����͸� ��ǥ ��ü�� �����ִ� ���� ��ƾ
GSErrCode		showGeometricalDataOnMorph (void)
{
	GSErrCode err = NoError;
	short	itemInd = DG_OK;
	std::string		tempString;

	// Selection Manager ���� ����
	API_SelectionInfo		selectionInfo;
	API_Neig				**selNeigs;
	long					nSel;
	API_Element				elem;
	API_ElementMemo			memo;
	API_ElemInfo3D			info3D;

	// ���� ���� ����
	const char*			prompt_localorigin = "���� ������ Ŭ���Ͻʽÿ�.";
	localOriginPointInfo.pos.x = 0;
	localOriginPointInfo.pos.y = 0;
	localOriginPointInfo.pos.z = 0;
	layerInd = 1;
	bLocalOrigin = false;
	szFont = 0.100;

	// ���̾�α� ȣ��
	do {
		itemInd = DGModalDialog (ACAPI_GetOwnResModule (), 32500, ACAPI_GetOwnResModule (), placerHandler, NULL);

		// ���ÿ��� ���� ��ư
		if (itemInd == BUTTON_SELECT_LOCALORIG) {
			BNZeroMemory (&localOriginPointInfo, sizeof (API_GetPointType));
			CHTruncate (prompt_localorigin, localOriginPointInfo.prompt, sizeof (localOriginPointInfo.prompt));
			localOriginPointInfo.changeFilter = false;
			localOriginPointInfo.changePlane  = false;
			err = ACAPI_Interface (APIIo_GetPointID, &localOriginPointInfo, NULL);
		}
	} while (itemInd == BUTTON_SELECT_LOCALORIG);

	if (itemInd != DG_OK)	return -1;	// �������� �ʰ� ����

	// ������ ��� ��������
	err = ACAPI_Selection_Get (&selectionInfo, &selNeigs, true);
	BMKillHandle ((GSHandle *) &selectionInfo.marquee.coords);
	if (err == APIERR_NOSEL) {
		ACAPI_WriteReport ("�ƹ� �͵� �������� �ʾҽ��ϴ�.", true);
		return err;
	}
	if (err != NoError) {
		BMKillHandle ((GSHandle *) &selNeigs);
		return err;
	}
	BNZeroMemory (&elem, sizeof (API_Element));
	BNZeroMemory (&memo, sizeof (API_ElementMemo));

	// ��ǥ ��ü ��ġ
	if (selectionInfo.typeID != API_SelEmpty) {
		nSel = BMGetHandleSize ((GSHandle) selNeigs) / sizeof (API_Neig);
		for (int xx = 0 ; xx < nSel && err == NoError ; ++xx) {
			elem.header.typeID = Neig_To_ElemID ((*selNeigs)[xx].neigID);
			elem.header.guid = (*selNeigs)[xx].guid;

			ACAPI_Element_Get (&elem);
			ACAPI_Element_GetMemo (elem.header.guid, &memo);

			// ����
			if (elem.header.typeID == API_MorphID) {
				ACAPI_Element_Get3DInfo (elem.header, &info3D);

				err = ACAPI_CallUndoableCommand ("��ǥ ��ü ��ġ", [&] () -> GSErrCode {
					tempString = format_string ("%s", "MIN ��");
					err = placeCoordinateLabel (info3D.bounds.xMin, info3D.bounds.yMin, info3D.bounds.zMin, true, tempString, layerInd);
					tempString = format_string ("%s", "MAX ��");
					err = placeCoordinateLabel (info3D.bounds.xMax, info3D.bounds.yMax, info3D.bounds.zMax, true, tempString, layerInd);
					tempString = format_string ("%s", "TMX �� [3,7,11]");
					err = placeCoordinateLabel (elem.morph.tranmat.tmx [3], elem.morph.tranmat.tmx [7], elem.morph.tranmat.tmx [11], true, tempString, layerInd);

					return err;
				});
			}
		}
	}
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
			// ���̾�α� Ÿ��Ʋ
			DGSetDialogTitle (dialogID, "��ü ��ǥ ��ġ");

			// ��ư
			DGSetItemText (dialogID, DG_OK, "��ġ");
			DGSetItemText (dialogID, DG_CANCEL, "����");

			// ���� ��Ʈ�� �ʱ�ȭ
			BNZeroMemory (&ucb, sizeof (ucb));
			ucb.dialogID = dialogID;
			ucb.type	 = APIUserControlType_Layer;
			ucb.itemID	 = USERCONTROL_LAYER;
			ACAPI_Interface (APIIo_SetUserControlCallbackID, &ucb, NULL);
			DGSetItemValLong (dialogID, USERCONTROL_LAYER, layerInd);

			// ���ÿ��� - üũ�ڽ�, ��ư
			DGSetItemText (dialogID, CHECKBOX_LOCALORIG, "���ÿ���");
			DGSetItemText (dialogID, BUTTON_SELECT_LOCALORIG, "���ÿ��� ����");
			DGSetItemValLong (dialogID, CHECKBOX_LOCALORIG, bLocalOrigin);

			// �۲�ũ�� - ��
			DGSetItemText (dialogID, LEFTTEXT_FONTSIZE, "�۲� ũ��");
			DGSetItemValDouble (dialogID, EDITCONTROL_FONTSIZE, szFont);

			break;
		
		case DG_MSG_CLICK:
			switch (item) {
				case BUTTON_SELECT_LOCALORIG:	// ���ÿ��� ���� ��ư
					// 2D/3D/�ܸ�/�Ը�/�ǳ��Ը� �󿡼� �� Ŭ�� ����
					// ȣ���ڿ��� ó����
					layerInd = (short)DGGetItemValLong (dialogID, USERCONTROL_LAYER);
					if (DGGetItemValLong (dialogID, CHECKBOX_LOCALORIG) == TRUE)
						bLocalOrigin = true;
					else
						bLocalOrigin = false;
					szFont = DGGetItemValDouble (dialogID, EDITCONTROL_FONTSIZE);
					break;
				
				case DG_OK:			// ��ġ ��ư
					// ���̾� ��ȣ
					layerInd = (short)DGGetItemValLong (dialogID, USERCONTROL_LAYER);

					// ���� �������� �ƴ��� ����
					if (DGGetItemValLong (dialogID, CHECKBOX_LOCALORIG) == TRUE)
						bLocalOrigin = true;
					else
						bLocalOrigin = false;

					// �۲� ũ��
					szFont = DGGetItemValDouble (dialogID, EDITCONTROL_FONTSIZE);

					break;
					
				case DG_CANCEL:		// ���� ��ư
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

// ��ǥ ���� ��ġ��
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

	// ���̺귯�� �̸� ����
	gsmName = L("��ǥ 19.gsm");

	// ��ü �ε�
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

	// �۾� �� ���� �ݿ�
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

	// ���̺귯���� �Ķ���� �� �Է�
	element.object.libInd = libPart.index;
	element.object.pos.x = xPos;
	element.object.pos.y = yPos;
	element.object.level = zPos - minusLevel;
	element.object.xRatio = aParam;
	element.object.yRatio = bParam;
	element.header.layer = layerInd;
	if (bLocalOrigin == true)
		memo.params [0][8].value.real = TRUE;	// ���� ���� Ȱ��ȭ
	else
		memo.params [0][8].value.real = FALSE;	// ���� ���� ��Ȱ��ȭ
	memo.params [0][10].value.real = szFont;		// �۲� ũ��
	memo.params [0][15].value.real = bComment;		// �ڸ�Ʈ On/Off
	GS::ucscpy (memo.params [0][16].value.uStr, GS::UniString (comment.c_str ()).ToUStr ().Get ());		// �ڸ�Ʈ ����

	// ���� ���� ��ǥ �Է� (Examples -> LibPart_Test)
	double** arrHdl = reinterpret_cast<double**>(memo.params [0][9].value.array);
	(*arrHdl)[0] = localOriginPointInfo.pos.x;
	(*arrHdl)[1] = localOriginPointInfo.pos.y;
	(*arrHdl)[2] = localOriginPointInfo.pos.z;

	// ��ü ��ġ
	ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);

	return	err;
}
