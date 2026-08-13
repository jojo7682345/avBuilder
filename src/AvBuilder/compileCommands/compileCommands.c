#define _POSIX_C_SOURCE 200809L
#include "compileCommands.h"
#include "AvUtils/filesystem/avDirectory.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <AvUtils/avEnvironment.h>


static bool32 isNixos(void){
	return access("/etc/NIXOS", F_OK)==0;
}

static const char* detectCompiler(const char* cmd){
	if(strstr(cmd, "clang")) return "clang";
	if(strstr(cmd, "gcc")) 	return "gcc";
	return NULL;
}

static void extractSourceFile(const char* cmd, char* out, size_t outz){
	const char* p = strstr(cmd, "-c");
	if(!p){ *out = '\0'; return; }
	p+= 3;

	if(*p == '"') p++;
	const char* end = strpbrk(p, " \t\n\"");
	size_t len = end ? (size_t)(end-p):strlen(p);
	if(len >= outz) len = outz - 1;
	strncpy(out, p, len);
	out[len] = '\0';
}

static void appendNixIncludes(const char* compiler, FILE* out){
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "%s -v -E -x c - < /dev/null 2>&1", compiler);

	FILE* pipe = popen(cmd, "r");
	if(!pipe) return;

	char line[512];
	bool32 inBlock = false;

	while(fgets(line, sizeof(line), pipe)){
		if(strstr(line, "#include <...>")){
			inBlock = true;
		}
		if(inBlock){
			if(strstr(line, "End of search list.")) break;

			char* path = line;
			while (*path==' ' || *path=='\t') path++;

			if(strncmp(path, "/nix/store/", 11) ==0){
				size_t len = strlen(path);
				if(path[len-1]=='\n')path[len-1] = '\0';
				fprintf(out, " -isystem%s", path);
			}
		}
	}
	pclose(pipe);
}

static FILE* json = NULL;
static bool32 firstEntry = true;

void ensureJsonOpen(){
	if(!json){
		json = fopen("compile_commands.json", "w");
		if(!json){
			perror("fopen compile_commands.json");
			return;
		}
		fprintf(json, "[\n");
	}
}

void addCommandToCompileCommands(const char *command){

	ensureJsonOpen();

	if(!firstEntry){
		fprintf(json, ",\n");
	}
	firstEntry = false;


	const char* compiler = detectCompiler(command);
	bool32 nix = isNixos();

	char srcFile[512];
	extractSourceFile(command, srcFile, sizeof(srcFile));
	if(srcFile[0]=='\0'){
		strcpy(srcFile, "(unkown)");
	}

	char cwd[4096];
	avGetCurrentDir(sizeof(cwd), cwd);

    for(uint32 i = 0; i < sizeof(cwd); i++){
        if(cwd[i]=='\\') cwd[i] = '/';
    }

	fprintf(json, "	{\n");
	fprintf(json, "		\"directory\": \"%s\",\n", cwd);
	fprintf(json, "		\"command\": \"");

	for(const char* p = command; *p; p++){
		if(*p == '"') fputc('\\', json);
		fputc(*p,json);
	}

	if(nix && compiler){
		FILE* tmp = tmpfile();
		if(tmp){
			appendNixIncludes(compiler, tmp);
			rewind(tmp);
			int c;
			while((c = fgetc(tmp)) != EOF) fputc(c, json);
			fclose(tmp);
		}
	}

	fprintf(json, "\",\n");
	fprintf(json, "		\"file\": \"%s\"\n", srcFile);
	fprintf(json, "	}");

	fflush(json);
}

void finalizeCompileCommands(void){
	if(!json)return;
	fprintf(json, "\n]\n");
	fclose(json);
	json=NULL;
}
