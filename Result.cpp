#include "Result.h"

#include <clang-c/Index.h>
#include <wx/intl.h>

wxString getCompletionString(CXCompletionString str)
{
    int numOfChunks = clang_getNumCompletionChunks(str);

    for (int i =0 ; i< numOfChunks; i++)
        if (clang_getCompletionChunkKind(str,i) == CXCompletionChunk_TypedText)
        {



            CXString str2 = clang_getCompletionChunkText(str,i);
            const char* str3 = clang_getCString(str2);

            wxString resulting = wxString(str3,wxConvUTF8);

            clang_disposeString(str2);

            return resulting;
        }


}


int getImageNum(CXCursorKind kind, CX_CXXAccessSpecifier spec)
{
    int result;


    switch(kind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_FunctionDecl:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = 13;

        else if (spec == CX_CXXProtected)
            result = 12;

        else if (spec == CX_CXXPrivate)
            result = 11;

        break;

    case CXCursor_FieldDecl:
        if (spec == CX_CXXPublic)
            result = 16;

        else if (spec == CX_CXXProtected)
            result = 15;

        else if (spec == CX_CXXPrivate)
            result = 14;
        break;


    case CXCursor_EnumDecl:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = 21;

        else if (spec == CX_CXXProtected)
            result = 20;

        else if (spec == CX_CXXPrivate)
            result = 19;
        break;

    case CXCursor_EnumConstantDecl:
        result = 22;
        break;


    case CXCursor_Constructor:
        if (spec == CX_CXXPublic || spec == CX_CXXInvalidAccessSpecifier)
            result = 7;

        else if (spec == CX_CXXProtected)
            result = 6;

        else if (spec == CX_CXXPrivate)
            result = 5;
        break;

    case CXCursor_ClassDecl:
    case CXCursor_StructDecl:
        if (spec == CX_CXXPublic)
            result = 4;

        else if (spec == CX_CXXProtected)
            result = 3;

        else if (spec == CX_CXXPrivate)
            result = 2;

        else if (spec == CX_CXXInvalidAccessSpecifier)
            result = 1;
        break;

    case CXCursor_Destructor:
        if (spec == CX_CXXPublic)
            result = 10;

        else if (spec == CX_CXXProtected)
            result = 9;

        else if (spec == CX_CXXPrivate)
            result = 8;
        break;

    case CXCursor_VarDecl:
        result = 16;
        break;

    case CXCursor_MacroDefinition:
        result = 35;
        break;

    default:
        result = 40;
        break;

    }


    return result;

}


Result::Result(CXCompletionResult result)
{

    CXCompletionString str = result.CompletionString;
    CXCursorKind kind = result.CursorKind;

    int type = getImageNum(kind,CX_CXXPublic);

    string = getCompletionString(str) + wxString::Format(_T("?%d"),type);
    rank = clang_getCompletionPriority(str);

    good = (clang_getCompletionAvailability(str) == CXAvailability_Available && string.GetChar(0) != '_');

}

bool Result::isGood()
{
    return good;
}

