//Самый быстрый Readline для огромных файлов
#include <stdio.h>
#include <fstream>
#include <windows.h>

DWORD Granularity;

#define MappedStep 1073741824
#define SeekStep 1048576

struct MappedFile {
	HANDLE hMap;
	BYTE *buf = 0;
	LONGLONG size;
	LONGLONG Position;
	LONGLONG MappedStart;
	LONGLONG MappedSize;
};

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

MappedFile MapFile(const char *path) {
	MappedFile m;
	HANDLE hFile = CreateFile(convertCharArrayToLPCWSTR(path), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) { printf("Unable to open hFile\n"); };
	m.hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!m.hMap) { printf("Create hMap mapping failed.\n"); };
	LARGE_INTEGER structLargeInt;
	GetFileSizeEx(hFile, &structLargeInt);
	m.size = structLargeInt.QuadPart;
	m.Position = 0;
	m.MappedStart = 0;
	m.MappedSize = 0;
	return m;
}

void Ask(MappedFile *m, LONG askedsize, LONGLONG* Rstart, LONGLONG* Rsize) {
	//урезаем осетра
	if (m->Position + askedsize > m->size) 
		askedsize = LONG(m->size - m->Position);
	//определяем, надо ли мапить
	if ((m->buf == 0) || (m->Position < m->MappedStart) || (m->Position + askedsize > m->MappedStart + m->MappedSize)) {//или вообще не мапили или правая или левая граница уползают		
		m->MappedStart = m->Position >> Granularity << Granularity;
		m->MappedSize = min(MappedStep, m->size - m->MappedStart);
		printf("MappedStart=%lld MappedSize=%lld\n", m->MappedStart, m->MappedSize);
		if (m->buf != 0) UnmapViewOfFile(m->buf);	
		m->buf = (BYTE*)MapViewOfFile(m->hMap, FILE_MAP_READ, m->MappedStart >> 32, m->MappedStart << 32 >> 32, m->MappedSize);
		if (m->buf == NULL)printf("MAPPING FAILED");
	}
	*Rstart = m->Position - m->MappedStart;
	*Rsize = askedsize;
}

bool ReadLine(MappedFile* m, BYTE** start, LONG* size) {//возвращает указатели на начало строки и размер, false - EOF
	LONGLONG Rstart;
	LONGLONG Rsize;	

	if (m->Position >= m->size) return false;
	int nd = 0;
	//Превращаем реальные размеры в относительные
	Ask(m, 256*256, &Rstart, &Rsize);
	//первый поиск
	BYTE* ptr = (BYTE*)memchr(m->buf + Rstart, 0x0A, Rsize);
	if (ptr == NULL) {
		//в любом случае возвращаем начало, и проматываем до конца или 0A.
		*start = (*m).buf + Rstart;
		*size = (LONG)Rsize;
		m->Position += Rsize;
		while (m->Position < m->size) {
			Ask(m, SeekStep, &Rstart, &Rsize);
			ptr = (BYTE*)memchr(m->buf + Rstart, 0x0A, Rsize);
			if (ptr != NULL) {
				m->Position += ptr - (m->buf + Rstart)+1;
				break;
			};
			m->Position += SeekStep;
		}
		return true;
	}
	else {
		*size = (LONG)(ptr - (m->buf + Rstart));
		if (*size > 0) 
			if (ptr[-1] == 0x0D) nd = 1;		
	};

	*start = m->buf + Rstart;
	m->Position += *size+1;
	*size = *size - nd;
	
	return true;
}

int main(int argc, const char **argv)
{
	SYSTEM_INFO sinf;
	GetSystemInfo(&sinf);
	if (sinf.dwAllocationGranularity == 65536)
		Granularity = 16; else { printf("Granularity error \n"); return EXIT_FAILURE; };

	std::ofstream out(argv[2]/*, std::ios::binary*/);
	std::ofstream out2(argv[3]/*, std::ios::binary*/);

	MappedFile f1 = MapFile(argv[1]);//На самом деле еще не отмапили
	BYTE* start;
	LONG size;

	while (ReadLine(&f1, &start, &size)) {
		for (LONG i = 0; i < size; i++) out << start[i];
		out << "\n";
	}

	f1.Position = 0;
	while (ReadLine(&f1, &start, &size)) {
		for (LONG i = 0; i < size; i++) out2 << start[i];
		out2 << "\n";
	}

	out.close();

	out2.close();

}
