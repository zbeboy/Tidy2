//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "PluginDefinition.h"
#include "menuCmdID.h"
#include <tidy.h>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
	setCommand(0, TEXT("Format"), doTidy, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
HWND getCurrentHScintilla()
{
	int which;
	SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
	return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

class TidyReadContext {
public:
	int nextPosition;
	int length;
	HWND hScintilla;
};

class TidyWriteContext {
public:
	HWND hScintilla;
};

int TIDY_CALL getByte(void *context) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);

	int returnByte = static_cast<unsigned char>(::SendMessage(tidyContext->hScintilla, SCI_GETCHARAT, tidyContext->nextPosition, 0));

	++(tidyContext->nextPosition);

	return returnByte;
}

void TIDY_CALL ungetByte(void *context, byte b) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);

	--(tidyContext->nextPosition);

}

Bool TIDY_CALL isInputEof(void *context) {
	TidyReadContext* tidyContext = reinterpret_cast<TidyReadContext*>(context);
	if (tidyContext->nextPosition >= tidyContext->length)
	{
		return yes;  // Urgh. Tidy enum.
	}
	else
	{
		return no;
	}
}

void TIDY_CALL putByte(void *context, byte b) {
	TidyWriteContext* tidyContext = reinterpret_cast<TidyWriteContext*>(context);
	::SendMessage(tidyContext->hScintilla, SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>(&b));
}

void createDefaultConfig(const char *configPath)
{
	FILE *defaultConfig = fopen(configPath, "wb");
	if (NULL != defaultConfig)
	{
		fprintf(defaultConfig, "%s",
			"indent: auto\r\n"
			"indent-spaces: 2\r\n"
			"wrap: 132\r\n"
			"markup: yes\r\n"
			"output-xml: yes\r\n"
			"input-xml: no\r\n"
			"numeric-entities: yes\r\n"
			"quote-marks: yes\r\n"
			"quote-nbsp: yes\r\n"
			"quote-ampersand: no\r\n"
			"break-before-br: no\r\n"
			"uppercase-tags: no\r\n"
			"uppercase-attributes: no\r\n"
			"new-inline-tags: cfif, cfelse, math, mroot, \r\n"
			"  mrow, mi, mn, mo, msqrt, mfrac, msubsup, munderover,\r\n"
			"  munder, mover, mmultiscripts, msup, msub, mtext,\r\n"
			"  mprescripts, mtable, mtr, mtd, mth\r\n"
			"new-blocklevel-tags: cfoutput, cfquery\r\n"
			"new-empty-tags: cfelse\r\n");
		fclose(defaultConfig);
	}
}

void doTidy()
{
	HWND currentScintilla = getCurrentHScintilla();
	int originalLength = static_cast<int>(::SendMessage(currentScintilla, SCI_GETLENGTH, 0, 0));
	LRESULT currentBufferID = ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
	int encoding = static_cast<int>(::SendMessage(nppData._nppHandle, NPPM_GETBUFFERENCODING, currentBufferID, 0));
	TidyDoc tidyDoc = tidyCreate();

	char configPath[MAX_PATH] = "./config";

	if (!tidyFileExists(tidyDoc, configPath))
	{
		createDefaultConfig(configPath);
	}

	tidyLoadConfigEnc(tidyDoc, configPath, "utf8");

	TidyReadContext context;
	context.nextPosition = 0;
	context.hScintilla = currentScintilla;
	context.length = originalLength;
	TidyInputSource inputSource;
	inputSource.getByte = getByte;
	inputSource.ungetByte = ungetByte;
	inputSource.sourceData = &context;
	inputSource.eof = isInputEof;
	char encodingString[30];

	switch (encoding)
	{
	case 1:  // UTF8
	case 2:  // UCS-2 BE, but stored in scintilla as UTF8
	case 3:  // UCS-2 LE, but stored in scintilla as UTF8
	case 6:  // UCS-2 BE No BOM - probably doesn't even exist
	case 7:  // UCS-2 LE No BOM - probably doesn't even exist
		strcpy(encodingString, "utf8");
		break;

	case 0:  // Ansi
	case 5:  // 7bit
		strcpy(encodingString, "raw");
		break;
	default:
		strcpy(encodingString, "raw");  // Assume it's some form of ASCII/Latin text
		break;

	}

	tidySetCharEncoding(tidyDoc, encodingString);
	tidyParseSource(tidyDoc, &inputSource);

	TidyOutputSink outputSink;
	TidyWriteContext writeContext;


	writeContext.hScintilla = reinterpret_cast<HWND>(::SendMessage(nppData._nppHandle, NPPM_CREATESCINTILLAHANDLE, 0, 0));
	if (NULL == writeContext.hScintilla)
	{
		::MessageBox(nppData._nppHandle, TEXT("Could not create new Scintilla handle for tidied output (You're probably low on memory!)"), TEXT("Tidy Errors"), MB_ICONERROR | MB_OK);
	}
	else
	{
		::SendMessage(writeContext.hScintilla, SCI_ALLOCATE, originalLength, 0);

		outputSink.putByte = putByte;
		outputSink.sinkData = &writeContext;


		tidyCleanAndRepair(tidyDoc);

		tidySaveSink(tidyDoc, &outputSink);
		int errorCount = tidyErrorCount(tidyDoc);

		if (errorCount > 0)
		{
			::MessageBox(nppData._nppHandle, TEXT("Errors were encountered tidying the document"), TEXT("Tidy Errors"), MB_ICONERROR | MB_OK);
		}
		else
		{
			char *text = reinterpret_cast<char*>(::SendMessage(writeContext.hScintilla, SCI_GETCHARACTERPOINTER, 0, 0));
			int length = static_cast<int>(::SendMessage(writeContext.hScintilla, SCI_GETLENGTH, 0, 0));
			::SendMessage(currentScintilla, SCI_BEGINUNDOACTION, 0, 0);
			::SendMessage(currentScintilla, SCI_CLEARALL, 0, 0);

			::SendMessage(currentScintilla, SCI_APPENDTEXT, static_cast<WPARAM>(length), reinterpret_cast<LPARAM>(text));
			::SendMessage(currentScintilla, SCI_ENDUNDOACTION, 0, 0);
		}

		::SendMessage(nppData._nppHandle, NPPM_DESTROYSCINTILLAHANDLE, 0, reinterpret_cast<LPARAM>(&writeContext.hScintilla));
	}

	tidyRelease(tidyDoc);
}



