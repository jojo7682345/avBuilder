#include <AvUtils/avString.h>
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/memory/avAllocator.h>
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/logging/avAssert.h>
#include <AvUtils/filesystem/avDirectoryV2.h>
#include <AvUtils/avEnvironment.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

const AvString configPath = AV_CSTRA(".config/AvBuilder/");  
const AvString templatePath = AV_CSTRA("templates/"); 

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
        avStringFree(&projectFileContent);
        avStringFree(&projectFileName); 
        goto loadingFailed;
    }

    AV_DS(AvDynamicArray, Token) tokens = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(Token), &tokens);
    if(!tokenizeProject(projectFileContent, projectFileContent, tokens)){
        avStringPrintf(AV_CSTR("Failed to tokenize project file %s\n"), projectFilePath);
        result = -1;
        avStringFree(&projectFileContent);
        avStringFree(&projectFileName); 
        goto tokenizingFailed;
    }

   
    Project project = AV_EMPTY;
    projectCreate(&project, projectFileName, projectFilePath, projectFileContent);
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
        AvString commandDebugFlag = AV_CSTR("--debugCommands");
        if(avStringStartsWith(argument, entryFlag)){
            AvString entry = {
                .chrs = argument.chrs + entryFlag.len,
                .len = argument.len - entryFlag.len,
            };
            memcpy(&options.entry, &entry, sizeof(AvString));
            avDynamicArrayRemove(i, arguments);
            i--;
        }
        if(avStringEquals(argument, commandDebugFlag)){
            options.commandDebug = true;
            avDynamicArrayRemove(i, arguments);
            i--;
        }
        
    }
    memcpy(&project.options, &options, sizeof(struct ProjectOptions));
    uint32 returnCode = runProject(&project, arguments);
    result = returnCode;

processingFailed:
parsingFailed:
    projectDestroy(&project);
tokenizingFailed:
    avDynamicArrayDestroy(tokens);
loadingFailed:
    avStringFree(&projectFileName);
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

void projectCreate(struct Project* project, AvString name, AvString file, AvString content){
    avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &(project->allocator));
    avDynamicArrayCreate(0, sizeof(struct VariableDescription), &project->variables);
    avDynamicArrayCreate(0, sizeof(struct VariableDescription), &project->constants);
    avDynamicArrayCreate(0, sizeof(struct FunctionDescription), &project->functions);
    avDynamicArrayCreate(0, sizeof(struct ImportDescription), &project->externals);
    avDynamicArrayCreate(0, sizeof(struct ImportDescription), &project->libraryAliases);
    avDynamicArrayCreate(0, sizeof(Project*), &project->importedProjects);
    avDynamicArrayCreate(0, sizeof(struct ConstValue*), &project->arrays);
    avStringClone(&project->name, name);
    memcpy(&project->projectFileContent, &content, sizeof(AvString));
    avStringClone(&project->projectFileName, file);
    
    project->localContext = NULL;
}
void projectDestroy(struct Project* project){
    avDynamicArrayDestroy(project->variables);
    avDynamicArrayDestroy(project->constants);
    avDynamicArrayDestroy(project->functions);
    avDynamicArrayDestroy(project->externals);
    avDynamicArrayDestroy(project->libraryAliases);
    avDynamicArrayForEachElement(Project*, project->importedProjects, {
        projectDestroy(element);
    });
    avDynamicArrayDestroy(project->importedProjects);
    avDynamicArrayDestroy(project->arrays);
    avAllocatorDestroy(&(project->allocator));
    avStringFree(&project->name);
    avStringFree(&project->projectFileContent);
    avStringFree(&project->projectFileName); 
}

static uint32 printUsage(const int argC, const char* argV[]){
    printf("Usage: avBuilder (options) [project file] (args)\n\n");
    printf("Options:\n");
    printf("  [project file] [args]                 Specify a project file to be processed\n");
    printf("  save [project file] (location)        Save the specified file to the local templates directory within the specified subdirectory\n");
    printf("  find [project file]                   Print the location of a saved project file\n");
    printf("  open [project file] [editor]          Opens the project file with a specified editor\n");
    printf("  remove [project file]                 Removes the specified project file\n");
    printf("  list                                  List the saved project files in the templates directory\n");
    printf("  help                                  Display this help message and exit\n");
    printf("\nExamples:\n");
    printf("  avBuilder myproject.project                   Process the myproject.project project file\n");
    printf("  avBuilder save myproject.project myproject    Saves the myproject.project file in the myproject subdirectory\n");
    printf("  avBuilder find myproject.project              Find the location of the myproject.project project file\n");
    printf("  avBuilder open myproject.project vim          Opens myproject.project with vim\n");
    printf("  avBuilder list                                List all saved project files\n");
    printf("  avBuilder help                                Show this help message\n");
    return 1;
}

static void getInConfigFolder(AvStringRef dest, AvString subDir){
    AvString homeDir = AV_EMPTY;
    avGetEnvironmentVariable(AV_CSTRA("HOME"), &homeDir);
    AvString tmpStr = AV_EMPTY;
    if(!avStringEndsWithChar(subDir, '/')){
        AvString str = AV_CSTRA("/");
        avStringUnsafeCopy(&tmpStr, str);
    }
    avStringJoin(dest, homeDir, AV_CSTRA("/"), configPath, subDir, tmpStr);
    avStringFree(&homeDir);
}

static void checkConfigFolder(){
    avStringDebugContextStart;

    AvString configDir = AV_EMPTY;
    getInConfigFolder(&configDir, (AvString)AV_EMPTY);


    if(!avDirectoryExists(configDir)){
        avMakeDirectoryRecursive(configDir);
    }

    avStringFree(&configDir);
    avStringDebugContextEnd;
}

static bool32 listInTree(AvPath path, AvString file, AvDynamicArray files){
    bool32 ret=  false;
    for(uint32 i = 0; i < path.contentCount; i++){
        AvPathNode node = path.content[i];
        if(node.type == AV_PATH_NODE_TYPE_FILE){
            if(avStringEndsWith(node.fullName, file)){
                AvString fileStr = AV_EMPTY;
                avStringClone(&fileStr, node.fullName);
                avDynamicArrayAdd(&fileStr, files);
                ret = true;
            }
            continue;
        }else if(node.type == AV_PATH_NODE_TYPE_DIRECTORY){
            AvPath child = AV_EMPTY;
            if(!avDirectoryOpen(node.name, &path, &child)){
                continue;
            }
            if(listInTree(child, file, files)){
                ret = true;
            }
            avDirectoryClose(&child);
            continue;
        }else{
            continue;
        }
    }
    return ret;

}

void freeString(void* data, uint64 size){
    avAssert(size==sizeof(AvString), "logic error");
    avStringFree((AvString*)data);
}

static uint32 openProject(const int argC, const char* argV[]){
    uint32 ret = 0;
    avStringDebugContextStart;
    AvString projectFile = AV_CSTR(argV[0]);
    AvString templatesDir = AV_EMPTY;
    getInConfigFolder(&templatesDir, templatePath);
    
    if(!avDirectoryExists(templatesDir)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto dirDoesNotExist;
    }
    AvPath templates = AV_EMPTY;
    if(!avDirectoryOpen(templatesDir, nullptr, &templates)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto unableToOpen;
    }
    AvDynamicArray files = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(AvString), &files);
    avDynamicArraySetDeallocateElementCallback(freeString, files);
    if(!listInTree(templates, projectFile, files)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto noFilesFound;
    }
    uint32 count = avDynamicArrayGetSize(files);
    uint32 file = 0;
    if(count==1){
        file = 0;
        goto openFile;
    }
    avStringPrintln(AV_CSTR("Multiple files found with the same name, please specify which"));
    for(uint32 index = 0; index < avDynamicArrayGetSize(files); index++) { 
        AvString element; avDynamicArrayRead(&element, index, (files));
        avStringPrintf(AV_CSTR("%i)  %s\n"), index, element);
    };
    avStringPrint(AV_CSTR("Enter file number: "));
    if(!scanf("%i", &file) || file >= count){
        avStringPrintln(AV_CSTR("Invalid number entered"));
        ret = -1;
        goto noFilesFound;
    }

openFile:
    AvString fileStr = AV_EMPTY;
    avDynamicArrayRead(&fileStr, file, files);
    char buffer[4096] = {0};
    avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%s %s"), AV_CSTR(argV[1]), fileStr);
    //avStringPrintln(AV_CSTR(buffer));
    ret = system(buffer);
noFilesFound:
    avDynamicArrayDestroy(files);
unableToOpen:
    avDirectoryClose(&templates);
dirDoesNotExist:
    avStringFree(&templatesDir);

    avStringDebugContextEnd;
    return ret;
}

static uint32 removeProject(const int argC, const char* argV[]){
    uint32 ret = 0;
    avStringDebugContextStart;
    AvString projectFile = AV_CSTR(argV[0]);
    AvString templatesDir = AV_EMPTY;
    getInConfigFolder(&templatesDir, templatePath);
    
    if(!avDirectoryExists(templatesDir)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto dirDoesNotExist;
    }
    AvPath templates = AV_EMPTY;
    if(!avDirectoryOpen(templatesDir, nullptr, &templates)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto unableToOpen;
    }
    AvDynamicArray files = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(AvString), &files);
    avDynamicArraySetDeallocateElementCallback(freeString, files);
    if(!listInTree(templates, projectFile, files)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto noFilesFound;
    }
    uint32 count = avDynamicArrayGetSize(files);
    uint32 file = 0;
    if(count==1){
        file = 0;
        goto openFile;
    }
    avStringPrintln(AV_CSTR("Multiple files found with the same name, please specify which"));
    for(uint32 index = 0; index < avDynamicArrayGetSize(files); index++) { 
        AvString element; avDynamicArrayRead(&element, index, (files));
        avStringPrintf(AV_CSTR("%i)  %s\n"), index, element);
    };
    avStringPrint(AV_CSTR("Enter file number: "));
    if(!scanf("%i", &file) || file >= count){
        avStringPrintln(AV_CSTR("Invalid number entered"));
        ret = -1;
        goto noFilesFound;
    }

openFile:
    AvString fileStr = AV_EMPTY;
    avDynamicArrayRead(&fileStr, file, files);
    ret = remove(fileStr.chrs);
noFilesFound:
    avDynamicArrayDestroy(files);
unableToOpen:
    avDirectoryClose(&templates);
dirDoesNotExist:
    avStringFree(&templatesDir);

    avStringDebugContextEnd;
    return ret;
}

static uint32 saveProject(const int argC, const char* argV[]){
    avStringDebugContextStart;
    uint32 ret = 0;

    AvString projectFile = AV_CSTR(argV[0]);
    AvString subDir = AV_EMPTY;
    if(argC >= 2){
        AvString tmpStr = AV_CSTR(argV[1]);
        avStringJoin(&subDir, templatePath, tmpStr);
    }else{
        avStringClone(&subDir, templatePath);
    }


    AvFile srcFile = AV_EMPTY;
    AvFile dstFile = AV_EMPTY;
    avFileHandleCreate(projectFile, &srcFile);
    if(!avFileExists(srcFile)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto openSrcFileFailed;
    }

    if(!avFileOpen(srcFile, AV_FILE_OPEN_READ_BINARY_DEFAULT)){
        avStringPrintf(AV_CSTR("Unabel to open %s\n"), projectFile);
        ret = -1;
        goto openSrcFileFailed;
    }
    checkConfigFolder();

    AvString saveDir = AV_EMPTY;
    AvString saveFile = AV_EMPTY;
    getInConfigFolder(&saveDir, subDir);
    if(!avDirectoryExists(saveDir)){
        avMakeDirectoryRecursive(saveDir);
    }
    AvArray paths = AV_EMPTY;
    avStringSplitOnChar(&paths, '/', projectFile);
    avStringJoin(&saveFile, saveDir, ((AvString*)paths.data)[paths.count-1]);
    avArrayFree(&paths);

    avFileHandleCreate(saveFile, &dstFile);
    if(!avFileOpen(dstFile, AV_FILE_OPEN_WRITE_BINARY_DEFAULT)){
        avStringPrintf(AV_CSTR("Unable to create new file %s\n"), saveFile);
        ret = -1;
        goto createDstFileFailed;
    }

    uint64 fileSize = avFileGetSize(srcFile);

    byte buffer[4096] = {0};

    uint64 bytesRemaining = fileSize;
    uint64 bytesWritten = 0;
    while(bytesRemaining){
        uint64 bytesRead = avFileRead(buffer, sizeof(buffer), srcFile);
        if(bytesRead==0){
            break;
        }
        bytesRemaining -= bytesRead;
        bytesWritten += avFileWrite(buffer, bytesRead, dstFile);
    }

    avFileClose(dstFile);
    avFileHandleDestroy(dstFile);

    avStringPrintf(AV_CSTR("Saved project file %s to %s\n"), projectFile, saveFile);

createDstFileFailed:
    avStringFree(&saveFile);
    avStringFree(&saveDir);
    avFileClose(srcFile);
openSrcFileFailed:
    avFileHandleDestroy(srcFile);
    avStringFree(&subDir);
    avStringDebugContextEnd;
    return ret;
}

static bool32 findInTree(AvPath path, AvString file, uint32 offset){
    bool32 ret=  false;
    for(uint32 i = 0; i < path.contentCount; i++){
        AvPathNode node = path.content[i];
        if(node.type == AV_PATH_NODE_TYPE_FILE){
            if(avStringEndsWith(node.fullName, file)){
                AvString str = {
                    .chrs = node.fullName.chrs + offset,
                    .len = node.fullName.len - offset,
                    .memory = nullptr,
                };
                avStringPrintln(str);
                ret = true;
            }
            continue;
        }else if(node.type == AV_PATH_NODE_TYPE_DIRECTORY){
            AvPath child = AV_EMPTY;
            if(!avDirectoryOpen(node.name, &path, &child)){
                continue;
            }
            if(findInTree(child, file, offset)){
                ret = true;
            }
            avDirectoryClose(&child);
            continue;
        }else{
            continue;
        }
    }
    return ret;

}

static uint32 findProject(const int argC, const char* argV[]){
    avStringDebugContextStart;
    uint32 ret = 0;
    AvString projectFile = AV_CSTR(argV[0]);
    AvString templatesDir = AV_EMPTY;
    getInConfigFolder(&templatesDir, templatePath);
    
    if(!avDirectoryExists(templatesDir)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto dirDoesNotExist;
    }
    AvPath templates = AV_EMPTY;
    if(!avDirectoryOpen(templatesDir, nullptr, &templates)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
        goto unableToOpen;
    }
    if(!findInTree(templates, projectFile, templatesDir.len)){
        avStringPrintf(AV_CSTR("Unable to find %s\n"), projectFile);
        ret = -1;
    }
unableToOpen:
    avDirectoryClose(&templates);
dirDoesNotExist:
    avStringFree(&templatesDir);
    avStringDebugContextEnd;
    return ret;
}

static bool32 printTree(AvPath path){
    for(uint32 i = 0; i < path.contentCount; i++){
        AvPathNode node = path.content[i];
        if(node.type == AV_PATH_NODE_TYPE_FILE){
            avStringPrintf(AV_CSTR("%s\n"), node.fullName);
        }else if(node.type == AV_PATH_NODE_TYPE_DIRECTORY){
            AvPath child = AV_EMPTY;
            if(!avDirectoryOpen(node.name, &path, &child)){
                return false;
            }
            printTree(child);
            avDirectoryClose(&child);

        }else{
            continue;
        }
    }
    return true;
}

static uint32 listProjects(const int argC, const char* argV[]){
    avStringDebugContextStart;
    uint32 ret = 0;
    AvString templatesDir = AV_EMPTY;
    getInConfigFolder(&templatesDir, templatePath);
    
    if(!avDirectoryExists(templatesDir)){
        ret = 0;
        goto dirDoesNotExist;
    }
    AvPath templates = AV_EMPTY;
    if(!avDirectoryOpen(templatesDir, nullptr, &templates)){
        ret = -1;
        goto unableToOpen;
    }

    if(!printTree(templates)){
        avStringPrintf(AV_CSTR("something went wrong enumerating files\n"));
        ret = -1;
    }

unableToOpen:
    avDirectoryClose(&templates);
dirDoesNotExist:
    avStringFree(&templatesDir);
    avStringDebugContextEnd;
    return ret;
}

static uint32 performProject(const int argC, const char* argV[]){
    AvDynamicArray arguments = NULL;
    avDynamicArrayCreate(argC, sizeof(AvString), &arguments);
    for(uint32 i = 1; i < argC; i++){
        AvString arg = AV_CSTR(argV[i]);
        avDynamicArrayAdd(&arg, arguments);
    }
    uint32 result = processProjectFile(AV_CSTR(argV[0]), arguments);
    avDynamicArrayDestroy(arguments);
    avStringDebugContextEnd;
    return result;
}

const struct Option{
    AvString tag;
    uint32 (*execute)(const int, const char*[]);
    uint8 argCount;
} options[] = {
    {AV_CSTRA("save"), saveProject, 1},
    {AV_CSTRA("find"), findProject, 1},
    {AV_CSTRA("list"), listProjects, 0},
    {AV_CSTRA("open"), openProject, 2},
    {AV_CSTRA("remove"), removeProject, 1},
    {AV_CSTRA("--help"), printUsage, 0},
    {AV_CSTRA("help"), printUsage, 0},
};

int main(int argC, const char* argV[]){
    avStringDebugContextStart;

    if(argC < 2){
        printf("no project file specified\n");
        printUsage(argC, argV);
        return -1;
    }

    AvString option = AV_CSTR(argV[1]);
    for(uint32 i = 0; i < (sizeof(options)/sizeof(struct Option)); i++){
        if(avStringEquals(options[i].tag, option)){
            if(argC < 2 + options[i].argCount){
                printf("not enough arguments specified\n");
                printUsage(argC, argV);
                return -1;
            }
            return options[i].execute(argC-2, argV+2);
        }
    }
    return performProject(argC-1, argV+1);
}