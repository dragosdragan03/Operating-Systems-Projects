// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

char* strcpy(char* destination, const char* source) {
	int i = 0;
	while (source[i] != '\0') {
		destination[i] = source[i];
		i++;
	}
	destination[i] = '\0';


	return destination;
}

char* strncpy(char* destination, const char* source, size_t len) {
	int i = 0;
	while (i < (int)len) {
		destination[i] = source[i];
		i++;
	}

	return destination;
}

char* strcat(char* destination, const char* source) {
	int len1 = strlen(destination);
	int len2 = strlen(source);

	int i;
	for (i = len1; i < len1 + len2; i++) {
		destination[i] = source[i - len1];
	}

	destination[i] = '\0';

	return destination;
}

char* strncat(char* destination, const char* source, size_t len) {
	int len1 = strlen(destination);

	int i;
	for (i = len1; i < len1 + (int)len && source[i - len1] != '\0'; i++) {
		destination[i] = source[i - len1];
	}

	destination[i] = '\0';

	return destination;
}

int strcmp(const char* str1, const char* str2) {
	int i = 0;
	while (i < (int)strlen(str1) && i < (int)strlen(str2)) {
		if (str1[i] < str2[i])
			return -1;
		else if (str1[i] > str2[i])
			return 1;
		i++;
	}
	if (strlen(str1) < strlen(str2))
		return -1;
	else if (strlen(str1) > strlen(str2))
		return 1;

	return 0;
}

int strncmp(const char* str1, const char* str2, size_t len) {
	int i = 0;
	while (i < (int)strlen(str1) && i < (int)strlen(str2) && i < (int)len) {
		if (str1[i] < str2[i])
			return -1;
		else if (str1[i] > str2[i])
			return 1;
		i++;
	}
	if (str1[len - 1] == str2[len - 1])
		return 0;
	else if (str1[len - 1] == '\0')
		return -1;

	return 1;
}

size_t strlen(const char* str) {
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char* strchr(const char* str, int c) {
	for (int i = 0; str[i] != '\0'; i++)
		if (str[i] == c)
			return (char*)(str + i);

	return NULL;
}

char* strrchr(const char* str, int c) {
	for (int i = strlen(str); i > 0; i--)
		if (str[i] == c)
			return (char*)(str + i);

	return NULL;
}

char* strstr(const char* haystack, const char* needle) {
	int len = strlen(needle);
	for (int i = 0; i < (int)strlen(haystack) - len; i++)
		if (!strncmp(haystack + i, needle, len))
			return (char*)(haystack + i);

	return NULL;
}

char* strrstr(const char* haystack, const char* needle) {
	int len = strlen(needle);
	for (int i = strlen(haystack) - len; i > 0; i--)
		if (!strncmp(haystack + i, needle, len))
			return (char*)(haystack + i);

	return NULL;
}

void* memcpy(void* destination, const void* source, size_t num) {
	char* ptr = (char*)destination;
	char* ptr2 = (char*)source;
	for (int i = 0; i < (int)num; i++)
		*(ptr + i) = *(ptr2 + i);

	return destination;
}

void* memmove(void* destination, const void* source, size_t num) {
	char* ptr = (char*)destination;
	char* ptr2 = (char*)source;
	for (int i = 0; i < (int)num; i++) {
		char aux = *(ptr2 + i);
		ptr[i] = aux;
	}

	return destination;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
	return strncmp(ptr1, ptr2, num);
}

void* memset(void* source, int value, size_t num) {
	char* ptr = (char*)source;
	for (int i = 0; i < (int)num; i++)
		*(ptr + i) = (char)value;

	return source;
}
