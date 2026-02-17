#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <string.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>

#include "avProjectLang.h"

void semanticError(struct Statement_S statement, Project* project, const char* message, ...){
    va_list args;
	va_start(args, message);

	avStringPrintf(AV_CSTR("Semantic Error in project %S at line %u:\n\t"), project->name, statement.line);
	avStringPrintfVA(AV_CSTR(message), args);

    va_end(args);
}

static bool32 processExpression(struct Expression_S expression, Project* project){
    switch(expression.type)
}

bool32 processProject(Project* project){

    // filter toplevel statements
    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S statement = project->statements[i];
        switch(statement.type){
            case STATEMENT_TYPE_BLOCK:
            case STATEMENT_TYPE_FOREACH:
            case STATEMENT_TYPE_IF:
            case STATEMENT_TYPE_RETURN:
                semanticError(statement, project, "invalid statement at top level");
                return false;
            case STATEMENT_TYPE_EXPRESSION:
                if(!processExpression(statement.expression, project)){
                    return false;
                }
            default:
                break;
        }

    }


}