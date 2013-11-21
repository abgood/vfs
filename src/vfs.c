/***********************************************************************************
 * Copyright (c) 2013, baickl(baickl@gmail.com)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * 
 * * Neither the name of the {organization} nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************************/
#include <vfs/vfs.h>
#include <vfs/pak.h>
#include <stdio.h>
#include <string.h>



typedef struct vfs_t
{
	var32		_M_count;
	var32		_M_maxcount;
	pak**		_M_paks;
}vfs;

static vfs * g_vfs = NULL;

static FILE* sfopen(const char* filename,const char* mode)
{
#ifndef _WIN32
	return fopen(filename,mode);
#else
	FILE* fp = NULL;
	var32 err;

	err = fopen_s(&fp,filename,mode);
	if( err == 0 )
	{
		return fp;
	}
	else
	{
		return NULL;
	}
#endif
}


var32 vfs_pak_sort_cmp(const void*a,const void*b)
{

	pak* _a;
	pak* _b;
	
	_a = *(pak**)a;
	_b = *(pak**)b;

	return stricmp(_a->_M_filename,_b->_M_filename);
}

var32 vfs_pak_search_cmp(const void*key,const void*item)
{

	char*_key;
	pak* _item;

	_key  = (char*)key;
	_item = *(pak**)item;
	return stricmp(_key,_item->_M_filename);
}

void vfs_pak_sort()
{
	qsort((void*)g_vfs->_M_paks,g_vfs->_M_count,sizeof(pak*),vfs_pak_sort_cmp);
}

var32 vfs_pak_search(const char* pakfile)
{
	var32 ret = -1;
	pak** p=NULL;

	if( !g_vfs || !pakfile)
		return -1;

	p = (pak**)bsearch(pakfile,g_vfs->_M_paks,g_vfs->_M_count,sizeof(pak*),vfs_pak_search_cmp);
	if( !p )
		return -1;

	return (p - g_vfs->_M_paks);
}

VFS_BOOL vfs_create()
{
	if( g_vfs )
		return VFS_TRUE;

	g_vfs = (vfs*)malloc(sizeof(vfs));
	if( !g_vfs )
		return VFS_FALSE;

	g_vfs->_M_count = 0;
	g_vfs->_M_maxcount = 0;
	g_vfs->_M_paks = NULL;

	return VFS_TRUE;
}

void vfs_destroy()
{
	var32 i;

	if( !g_vfs )
		return ;

	for( i = 0; i<g_vfs->_M_count; ++i )
	{
		pak_close(g_vfs->_M_paks[i]);
	}

	VFS_SAFE_FREE(g_vfs->_M_paks);
	VFS_SAFE_FREE(g_vfs);
}

VFS_BOOL vfs_pak_add(const char* pakfile )
{
	pak* p;
	pak** _paks;

	if(vfs_pak_search(pakfile) >= 0 )
		return VFS_TRUE;

	p = pak_open(pakfile);
	if( !p )
		return VFS_FALSE;

	if( g_vfs->_M_count >= g_vfs->_M_maxcount )
	{
		if( g_vfs->_M_count == 0 )
		{
			g_vfs->_M_maxcount = 32;
			g_vfs->_M_paks = (pak**)malloc(g_vfs->_M_maxcount*sizeof(pak*));
			if( !g_vfs->_M_paks )
			{
				g_vfs->_M_maxcount = 0;
				pak_close(p);
				return VFS_FALSE;
			}
		}
		else
		{
			g_vfs->_M_maxcount += 32;
			_paks = (pak**)realloc(g_vfs->_M_paks,g_vfs->_M_maxcount*sizeof(pak*));
			if( !_paks )
			{
				g_vfs->_M_maxcount -= 32;
				pak_close(p);
				return VFS_FALSE;
			}

			/*
			 * 拿到新地址
			 */
			g_vfs->_M_paks = _paks;
		}
	}
	
	/* 新增 */
	g_vfs->_M_paks[g_vfs->_M_count++] = p;
	vfs_pak_sort();

	return VFS_TRUE;
}

VFS_BOOL vfs_pak_del(const char* pakfile )
{

	var32 index;

	if( !g_vfs )
		return VFS_FALSE;

	if( !pakfile )
		return VFS_FALSE;

	index = vfs_pak_search(pakfile);
	if( index < 0 || index >= g_vfs->_M_count )
		return VFS_FALSE;

	pak_close(g_vfs->_M_paks[index]);
	g_vfs->_M_paks[index]= g_vfs->_M_paks[g_vfs->_M_count -1];
	--g_vfs->_M_count;

	vfs_pak_sort();
	return VFS_TRUE;
}


var32 vfs_pak_get_count()
{
	if( g_vfs )
		return g_vfs->_M_count;
	return 0;
}

pak* vfs_pak_get_index(var32 idx )
{
	if( g_vfs && idx >= 0 && idx <= g_vfs->_M_count )
		return g_vfs->_M_paks[idx];

	return NULL;
}

pak* vfs_pak_get_name(const char* pakfile)
{	
	pak* p;
	var32 index;

	index = vfs_pak_search(pakfile);
	if( index < 0 || index >= g_vfs->_M_count )
		return NULL;

	p = g_vfs->_M_paks[index];
	return p;
}

VSF_FILE* vfs_fcreate(void*buf,uvar64 size)
{
	VSF_FILE* vff;

	vff = (VSF_FILE*)malloc(sizeof(VSF_FILE));
	if( !vff )
	{
		VFS_SAFE_FREE(buf);
		return NULL;
	}

	if( !size)
	{
		vff->_M_buffer = 0;
		vff->_M_size = 0;
		vff->_M_position = 0;
	}
	else
	{
		vff->_M_buffer = (void*)malloc(size);
		if( !vff->_M_buffer )
		{
			VFS_SAFE_FREE(buf);
			return NULL;
		}
		vff->_M_position = 0;
		vff->_M_size = size;

		if( buf )
			memcpy(vff->_M_buffer,buf,(size_t)size);
	}
	
	return vff;
}

VSF_FILE* vfs_fopen(const char* file )
{
	var32 i,index ;
	uvar64 size;
	void* buf;
	pak_iteminfo* iteminfo;
	VSF_FILE* vff;

	FILE* fp;

	if( !file )
		return NULL;

	/* 先尝试从包里读取 */
	if( g_vfs )
	{
		for( i = 0; i<g_vfs->_M_count; ++i )
		{
			index = pak_item_locate(g_vfs->_M_paks[i],file);
			if(index < 0 )
				continue;

			iteminfo = pak_item_getinfo(g_vfs->_M_paks[i],index);
			if( !iteminfo )
				continue;

			size = iteminfo->_M_size;
			buf = (void*)malloc(size);
			if( !buf )
				return NULL;

			if( VFS_TRUE != pak_item_unpack_index(g_vfs->_M_paks[i],index,buf,size) ) 
			{
				VFS_SAFE_FREE(buf);
				return NULL;
			}

			vff = vfs_fcreate(0,0);
			if( !vff )
			{
				VFS_SAFE_FREE(buf);
				return NULL;
			}
			vff->_M_buffer = buf;
			vff->_M_size = size;
			vff->_M_position = 0;

			return vff;
		}
	}


	/* 包里读取不出来，在本地读取 */
	fp = sfopen(file,"rb");
	if( fp )
	{
		VFS_FSEEK(fp,0,SEEK_END);
		size = VFS_FTELL(fp);
		VFS_FSEEK(fp,0,SEEK_SET);
		buf = NULL;

		if( size > 0 )
		{
			buf = (void*)malloc(size);
			if( !buf )
			{
				VFS_SAFE_FCLOSE(fp);
				return NULL;
			}

			if( !VFS_CHECK_FWRITE(fp,buf,size) )
			{
				VFS_SAFE_FCLOSE(fp);
				VFS_SAFE_FREE(buf);
				return NULL;
			}

			VFS_SAFE_FCLOSE(fp);
		}

		vff = vfs_fcreate(0,0);
		if( !vff )
		{
			VFS_SAFE_FREE(buf);
			return NULL;
		}
		vff->_M_buffer = buf;
		vff->_M_size = size;
		vff->_M_position = 0;

		return vff;
	}

	return NULL;
}

void vfs_fclose(VSF_FILE* file)
{
	if( file )
	{
		VFS_SAFE_FREE(file->_M_buffer);
		VFS_SAFE_FREE(file);
	}
}


VFS_BOOL vfs_feof(VSF_FILE* file )
{
	if( file && file->_M_position >= file->_M_size )
		return VFS_TRUE;
	else
		return VFS_FALSE;
}

uvar64 vfs_ftell(VSF_FILE* file)
{
	if( file )
		return file->_M_position;

	return 0;
}

uvar64 vfs_fsize( VSF_FILE* file )
{
	if( !file )
		return 0;

	return file->_M_size;
}

uvar64 vfs_fseek(VSF_FILE* file,var64 pos, var32 mod )
{
	uvar64 _pos;
	if( !file )
		return -1;

	if( mod == SEEK_CUR )
	{
		_pos = file->_M_position + pos;
		if( _pos >= 0 && _pos < file->_M_size  )
			file->_M_position = pos;
	}
	else if( mod == SEEK_END )
	{
		_pos = file->_M_size -1 + pos;
		if( _pos >= 0 && _pos < file->_M_size )
			file->_M_position = _pos;
	}
	else
	{
		if( pos >= 0 && pos < (var64)file->_M_size  )
			file->_M_position = pos;
	}

	return vfs_ftell(file);
}

uvar64 vfs_fread( void* buf , uvar64 size , uvar64 count , VSF_FILE*fp )
{

	uvar64 i;
	uvar64 realread;
	char* p;

	if(!fp)
		return 0;

	if(!buf)
		return 0;
	
	if(!size || !count )
		return 0;
	
	p = (char*)buf;
	realread = 0;

	for( i = 0; i<count; ++i )
	{
		if( vfs_feof(fp) )
			break;

		if( (fp->_M_size - fp->_M_position ) >= size )
		{
			memcpy(p,&((char*)fp->_M_buffer)[fp->_M_position],(size_t)size);
			fp->_M_position += size;
			p += size;
			realread += size;
		}
	}

	return realread;
}

uvar64 vfs_fwrite(void* buf , uvar64 size , uvar64 count , VSF_FILE*fp )
{
	uvar64 i;
	uvar64 realwrite;
	char* p;
	
	void* tmp;
	
	if( !fp )
		return 0;

	if( !buf )
		return 0;

	if( !size || !count )
		return 0;

	p = (char*)buf;
	realwrite = 0;
	for( i = 0; i<count; ++i )
	{
		if( (fp->_M_size - fp->_M_position ) < size )
		{
			tmp = (void*)realloc(fp->_M_buffer,fp->_M_size + size );
			if( !tmp )
				break;

			fp->_M_buffer = tmp;
			fp->_M_size += size;
		}

		memcpy(&((char*)fp->_M_buffer)[fp->_M_position],p,(size_t)size);
		fp->_M_position += size;
		p += size;
		realwrite += size;

	}

	return realwrite;
}


VFS_BOOL vfs_fsave(VSF_FILE* file,const char* saveas)
{

	FILE* fp;
	uvar64 offset;

	uvar64 realsize ;
	char buf[512+1];

	if( !file || !saveas  )
		return VFS_FALSE;
	
	fp = sfopen(saveas,"wb+");
	if( !fp )
		return VFS_FALSE;

	offset = vfs_ftell(file);
	while( !vfs_feof(file) )
	{
		realsize = vfs_fread(buf,1,512,file);
		if( realsize > 0 )
		{
			buf[realsize] = 0;
			if( fwrite(buf,1,(size_t)realsize,fp) != realsize )
			{
				VFS_SAFE_FCLOSE(fp);
				vfs_fseek(file,offset,SEEK_SET);
				remove(saveas);
				return VFS_FALSE;
			}
		}

		VFS_SAFE_FCLOSE(fp);
		vfs_fseek(file,offset,SEEK_SET);
	}

	VFS_SAFE_FCLOSE(fp);
	return VFS_TRUE;
}
