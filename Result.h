#ifndef RESULT_H
#define RESULT_H


#include <wx/string.h>
#include <clang-c/Index.h>



struct Result
{
    int rank;
    wxString string;
    bool good;


    bool operator<(const Result& other) const
    {
        if (string.Upper() < other.string.Upper())
            return true;

        return false;


    }

    Result()
    {
    }

    bool isGood();


    Result(CXCompletionResult result);

};
#endif // RESULT_H
