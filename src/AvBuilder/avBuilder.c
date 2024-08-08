#include <AvUtils/avString.h>
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/memory/avAllocator.h>
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "avBuilder.h"


#define TOKEN_KEYWORD(symbol) AV_CSTRA(symbol),
#define TOKEN_PUNCTUATOR(symbol)
#define TOKEN(type, token, symbol) TOKEN_##type(symbol)
const AvString keywords[] = {
    LIST_OF_TOKENS
};
#undef TOKEN_KEYWORD
#undef TOKEN_PUNCTUATOR
#undef TOKEN
const uint32 keywordCount = sizeof(keywords)/sizeof(AvString);


#define TOKEN_KEYWORD(symbol) 
#define TOKEN_PUNCTUATOR(symbol) AV_CSTRA(symbol),
#define TOKEN(type, token, symbol) TOKEN_##type(symbol)
const AvString punctuators[] = {
    LIST_OF_TOKENS
};
#undef TOKEN_KEYWORD
#undef TOKEN_PUNCTUATOR
#undef TOKEN
const uint32 punctuatorCount = sizeof(punctuators)/sizeof(AvString);



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wjump-misses-init"
uint32 processProjectFile(const AvString projectFilePath, AvDynamicArray arguments){
    avStringDebugContextStart;
    uint32 result = true;

    AvString projectFileContent = AV_EMPTY;
    AvString projectFileName = AV_EMPTY;
    if(!loadProjectFile(projectFilePath, &projectFileContent, &projectFileName)){
        avStringPrintf(AV_CSTR("Failed to load project file %s\n"), projectFilePath);
        result = -1;
        goto loadingFailed;
    }

    AV_DS(AvDynamicArray, Token) tokens = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(Token), &tokens);
    if(!tokenizeProject(projectFileContent, projectFileContent, tokens)){
        avStringPrintf(AV_CSTR("Failed to tokenize project file %s\n"), projectFilePath);
        result = -1;
        goto tokenizingFailed;
    }
    //printTokenList(tokens);
   
    Project project = AV_EMPTY;
    projectCreate(&project, projectFileName);
    struct ProjectStatementList* statements = nullptr;
    if(!parseProject(tokens, (void**)&statements, &project)){
        avStringPrintf(AV_CSTR("Failed to parse project file %s\n"), projectFilePath);
        result = -1;
        goto parsingFailed;
    }
    
    if(!processProject(statements, &project)){
        avStringPrintf(AV_CSTR("Failed to perform processing on project file %s\n"), projectFilePath);
        result = -1;
        goto processingFailed;
    }

    struct ProjectOptions options = {0};
    avDynamicArraySetAllowRelocation(true, arguments);

    for(uint32 i = 0; i < avDynamicArrayGetSize(arguments); i++){
        AvString argument = AV_EMPTY;
        avDynamicArrayRead(&argument, i, arguments);
        AvString entryFlag = AV_CSTR("--entry=");
        if(avStringStartsWith(argument, entryFlag)){
            AvString entry = {
                .chrs = argument.chrs + entryFlag.len,
                .len = argument.len - entryFlag.len,
            };
            memcpy(&options.entry, &entry, sizeof(AvString));
            avDynamicArrayRemove(i, arguments);
            i--;
        }
        
    }

    uint32 returnCode = runProject(&project, arguments, options);
    result = returnCode;

processingFailed:
parsingFailed:
    projectDestroy(&project);
tokenizingFailed:
    avDynamicArrayDestroy(tokens);
loadingFailed:
    avStringFree(&projectFileName); 
    avStringFree(&projectFileContent);

    avStringDebugContextEnd;
    return result;
}
#pragma GCC diagnostic pop

void startLocalContext(struct Project* project, bool32 inherit){
    LocalContext* context = avAllocate(sizeof(LocalContext), "local context");
    context->previous = project->localContext;
    avDynamicArrayCreate(0, sizeof(struct VariableDescription), &context->variables);
    project->localContext = context;
    context->inherit = inherit;
}
void endLocalContext(struct Project* project){
    LocalContext* context = project->localContext;
    avDynamicArrayDestroy(context->variables);
    project->localContext = context->previous;
    avFree(context);
}

void projectCreate(struct Project* project, AvString name){
    avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &(project->allocator));
    avDynamicArrayCreate(0, sizeof(struct VariableDescription), &project->variables);
    avDynamicArrayCreate(0, sizeof(struct VariableDescription), &project->constants);
    avDynamicArrayCreate(0, sizeof(struct FunctionDescription), &project->functions);
    avDynamicArrayCreate(0, sizeof(struct FunctionDescription), &project->externals);
    avDynamicArrayCreate(0, sizeof(struct ConstValue*), &project->arrays);
    avStringClone(&project->name, name);
    project->localContext = NULL;
}
void projectDestroy(struct Project* project){
    avDynamicArrayDestroy(project->variables);
    avDynamicArrayDestroy(project->constants);
    avDynamicArrayDestroy(project->functions);
    avDynamicArrayDestroy(project->externals);
    avDynamicArrayDestroy(project->arrays);
    avAllocatorDestroy(&(project->allocator));
    avStringFree(&project->name);
}

int main(int argC, const char* argV[]){
    avStringDebugContextStart;

    if(argC < 2){
        avStringPrintf(AV_CSTR("No project file specified\n"));
        return -1;
    }

    AvDynamicArray arguments = NULL;
    avDynamicArrayCreate(argC-2, sizeof(AvString), &arguments);
    for(uint32 i = 2; i < argC; i++){
        AvString arg = AV_CSTR(argV[i]);
        avDynamicArrayAdd(&arg, arguments);
    }
    uint32 result = processProjectFile(AV_CSTR(argV[1]), arguments);
    if(result==0){
        avStringPrintf(AV_CSTR("Successfully parsed project file\n"));
    }

    avDynamicArrayDestroy(arguments);
    avStringDebugContextEnd;
    return result;
}