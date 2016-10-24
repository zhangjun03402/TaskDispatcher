#ifndef WIN32
#include <iconv.h>
#include <unistd.h>

void Sleep(int stm_ms)
{
	usleep(stm_ms * 1000);
}
#endif



enum EncodingConversionType{
	UTF8_GB = 0,
	GB_UTF8 = 1
};
/**将UTF8编码转换为GB2312编码*/
char * ConvertAndCopy(char* src_str, char* dest_str, int dest_buffer_size, EncodingConversionType direction)
{

#ifdef WIN32
	WCHAR *strSrc;
	UINT page = direction == UTF8_GB ? CP_UTF8 : CP_ACP;
	//获得临时变量的大小
	int i = MultiByteToWideChar(page, 0, src_str, -1, NULL, 0);
	strSrc = new WCHAR[i + 1];
	//convert to wchar string
	MultiByteToWideChar(page, 0, src_str, -1, strSrc, i);
	//获得临时变量的大小
	page = direction == UTF8_GB ? CP_ACP : CP_UTF8;
	i = WideCharToMultiByte(direction, 0, strSrc, -1, NULL, 0, NULL, NULL);
	//convert to char string
	WideCharToMultiByte(direction, 0, strSrc, -1, dest_str, i, NULL, NULL);
	delete[]strSrc;
	return dest_str;
#else

	iconv_t cd;
	const char *source_encoding = "utf-8";
	const char *dest_encoding = "gb2312";
	if (direction == GB_UTF8){
		const char *tmp_encoding = source_encoding;
		source_encoding = dest_encoding;
		dest_encoding = tmp_encoding;
	}
	else if (direction != UTF8_GB){
		return NULL;
	}

	if ((cd = iconv_open(source_encoding, dest_encoding)) == 0)
		return NULL;

	size_t sourcelen = strlen(src_str);
	size_t destlen = dest_buffer_size;
	char **source = &src_str;
	memset(dest_str, 0, dest_buffer_size);
	char **dest = &dest_str;
	if (-1 == iconv(cd, source, &sourcelen, dest, &destlen))
		return NULL;
	iconv_close(cd);
	return dest_str;

#endif
}