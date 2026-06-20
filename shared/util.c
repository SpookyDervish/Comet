#include "../include/util.h"

char* getFileContents(const char* filename) {
    // https://stackoverflow.com/questions/3747086/reading-the-whole-text-file-into-a-char-array-in-c
    FILE* fp;
    long lSize;
    char* file;

    fp = fopen(filename, "rb");
    if (!fp) {
        perror(filename);
        exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    file = calloc(1, lSize + 1);
    if (!file) {
        fclose(fp);
        fprintf(stderr, "memory allocation fail when reading file %s\n", filename);
        exit(1);
    }

    if (1!=fread(file, lSize, 1, fp)) {
        fclose(fp);
        free(file);
        fputs("couldn't read entire file", stderr);
        exit(1);
    }

    // we done
    fclose(fp);

    return file;
}

char* getLibsDir() {
    const char* home = getHomeDir();
    if (!home)
        return NULL;

    #ifdef _WIN32
    char* seperator = "\\";
    #else
    char* seperator = "/";
    #endif
    
    char* cometDir = ".comet/libs";

    size_t requiredLen = strlen(home) + strlen(seperator) + strlen(cometDir) + 1;

    char* fullPath = malloc(sizeof(requiredLen));
    if (!fullPath)
        return NULL;

    snprintf(fullPath, requiredLen, home, seperator, cometDir);

    return fullPath;
}

const char* getHomeDir() {
    const char* homeDir;

    #ifdef _WIN32
    // Windows home directory path (e.g., C:\Users\Username)
    homeDir = getenv("USERPROFILE");
    if (!homeDir) {
        // Fallback for older or specific Windows configurations
        homeDir = getenv("HOMEPATH"); 
    }
    #else
        // Linux and macOS home directory path (e.g., /home/username)
        homeDir = getenv("HOME");
    #endif

    return homeDir;
}

// Thank you to https://creativeandcritical.net/str-replace-c for this function :)
char *repl_str(const char *str, const char *from, const char *to) {

	/* Adjust each of the below values to suit your needs. */

	/* Increment positions cache size initially by this number. */
	size_t cache_sz_inc = 16;
	/* Thereafter, each time capacity needs to be increased,
	 * multiply the increment by this factor. */
	const size_t cache_sz_inc_factor = 3;
	/* But never increment capacity by more than this number. */
	const size_t cache_sz_inc_max = 1048576;

	char *pret, *ret = NULL;
	const char *pstr2, *pstr = str;
	size_t i, count = 0;
	#if (__STDC_VERSION__ >= 199901L)
	uintptr_t *pos_cache_tmp, *pos_cache = NULL;
	#else
	ptrdiff_t *pos_cache_tmp, *pos_cache = NULL;
	#endif
	size_t cache_sz = 0;
	size_t cpylen, orglen, retlen, tolen, fromlen = strlen(from);

	/* Find all matches and cache their positions. */
	while ((pstr2 = strstr(pstr, from)) != NULL) {
		count++;

		/* Increase the cache size when necessary. */
		if (cache_sz < count) {
			cache_sz += cache_sz_inc;
			pos_cache_tmp = realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
			if (pos_cache_tmp == NULL) {
				goto end_repl_str;
			} else pos_cache = pos_cache_tmp;
			cache_sz_inc *= cache_sz_inc_factor;
			if (cache_sz_inc > cache_sz_inc_max) {
				cache_sz_inc = cache_sz_inc_max;
			}
		}

		pos_cache[count-1] = pstr2 - str;
		pstr = pstr2 + fromlen;
	}

	orglen = pstr - str + strlen(pstr);

	/* Allocate memory for the post-replacement string. */
	if (count > 0) {
		tolen = strlen(to);
		retlen = orglen + (tolen - fromlen) * count;
	} else	retlen = orglen;
	ret = malloc(retlen + 1);
	if (ret == NULL) {
		goto end_repl_str;
	}

	if (count == 0) {
		/* If no matches, then just duplicate the string. */
		strcpy(ret, str);
	} else {
		/* Otherwise, duplicate the string whilst performing
		 * the replacements using the position cache. */
		pret = ret;
		memcpy(pret, str, pos_cache[0]);
		pret += pos_cache[0];
		for (i = 0; i < count; i++) {
			memcpy(pret, to, tolen);
			pret += tolen;
			pstr = str + pos_cache[i] + fromlen;
			cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
			memcpy(pret, pstr, cpylen);
			pret += cpylen;
		}
		ret[retlen] = '\0';
	}

end_repl_str:
	/* Free the cache and return the post-replacement string,
	 * which will be NULL in the event of an error. */
	free(pos_cache);
	return ret;
}

char *getLineInString(const char *str, int target_line) {
	const char *current = str;
    int line_num = 1;

    // Advance through the string until reaching the target line
    while (current && line_num < target_line) {
        current = strchr(current, '\n');
        if (current) {
            current++; // Move past the '\n' character
            line_num++;
        }
    }

    // If the target line was found
    if (current && line_num == target_line) {
        const char *next_newline = strchr(current, '\n');
        
        // Calculate length of the target line
        size_t length = next_newline ? (size_t)(next_newline - current) : strlen(current);
        
        // Allocate memory (+1 for the null terminator)
        char *result = malloc(length + 1);
        if (result == NULL) {
            return NULL; // Return NULL if memory allocation fails
        }
        
        // Copy the line into the new buffer and null-terminate it
        strncpy(result, current, length);
        result[length] = '\0';
        
        return result;
    }

    return NULL; // Return NULL if the line number does not exist
}

char* repeatString(const char* str, int times) {
    // handle zero
    if (times < 1) {
        char* result = malloc(1);
        if (result == NULL) 
            return NULL;
        
        result[0] = 0;
        return result;
    }

    char* result = malloc(strlen(str) * times + 1);
    if (result == NULL)
        return NULL;

    strcpy(result, str);

    for (int i = 1; i < times; i++) {
        strcat(result, str);
    }

    return result;
}