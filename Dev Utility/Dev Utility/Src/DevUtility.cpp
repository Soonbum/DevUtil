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

static bool		bLocalOrigin;	// true�̸� ���� ����, false�̸� �۷ι� ����
static double	szFont;			// �۲� ũ��
static short	layerInd;		// ���̾� �ε���
// ... ���� ���� ��ǥ ����ü: Coords
// ... vertices ����Ʈ: Coords �迭
// ... ��ü ����Ʈ: API_Element

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

// 1�� �޴�: ��ǥ ��ü�� ��ġ�ϴ� ���� ��ƾ
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
			DGSetItemValLong (dialogID, USERCONTROL_LAYER, 1);

			// ���ÿ��� - üũ�ڽ�, ��ư
			DGSetItemText (dialogID, CHECKBOX_LOCALORIG, "���ÿ���");
			DGSetItemText (dialogID, BUTTON_SELECT_LOCALORIG, "���ÿ��� ����");

			// ��ü���� - ��ư
			DGSetItemText (dialogID, BUTTON_SELECT_OBJECT, "��ü ����");

			// �۲�ũ�� - ��
			DGSetItemText (dialogID, LEFTTEXT_FONTSIZE, "�۲� ũ��");
			DGSetItemValDouble (dialogID, EDITCONTROL_FONTSIZE, 0.100);		// �⺻�� 100

			break;

		case DG_MSG_CHANGE:
			break;

		case DG_MSG_CLICK:
			switch (item) {
				case DG_OK:			// ��ġ ��ư
					// ... ���� '��ü ����'���� ������ ��ġ���� ����
					break;
				
					// ���̺귯���� ������ ���� �����ϱ� ***

					// �������ٰ� ��ǥ ���̺귯�� ��ü ��ġ

				case DG_CANCEL:		// ���� ��ư
					item = DG_CANCEL;
					break;
				
				case BUTTON_SELECT_LOCALORIG:	// ���ÿ��� ���� ��ư
					// ... 2D/3D/�ܸ�/�Ը�/�ǳ��Ը� �󿡼� �� Ŭ�� ����
					break;
				
				case BUTTON_SELECT_OBJECT:		// ��ü ���� ��ư
					// ������ ��� ��������
					// ... ACAPI_Selection_Get

					// ... �������� ������ ��ü ����
						// ���������� ���
						// ������ ���

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
// ��ǥ ���� ��ġ��
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

	// ���̺귯���� �Ķ���� �� �Է�
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

	// ��ü ��ġ
	ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);

	return	err;
}
*/