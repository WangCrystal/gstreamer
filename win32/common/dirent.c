/*
 * dirent.c
 *
 * Derived from DIRLIB.C by Matt J. Weinstein 
 * This note appears in the DIRLIB.H
 * DIRLIB.H by M. J. Weinstein   Released to public domain 1-Jan-89
 *
 * Updated by Jeremy Bettis <jeremy@hksys.com>
 * Significantly revised and rewinddir, seekdir and telldir added by Colin
 * Peters <colin@fu.is.saga-u.ac.jp>
 *
 * Resource leaks fixed by <steve.lhomme@free.fr>
 *
 *	
 * $Revision$
 * $Author$
 * $Date$
 *
 */  
    
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include <dirent.h>
#include <gtchar.h>
#define SUFFIX	_T("*")
#define	SLASH	_T("\\")
    
#include <stdio.h>
    
#define WIN32_LEAN_AND_MEAN
#include <windows.h>            /* for GetFileAttributes */
    
/*
 * opendir
 *
 * Returns a pointer to a DIR structure appropriately filled in to begin
 * searching a directory.
 */ 
    _TDIR * 
{
  
  

  
  
  
    
    
  
  
    
    
  
  
      /* Attempt to determine if the given path really is a directory. */ 
      rc = GetFileAttributes (szPath);
  
    
        /* call GetLastError for more error info */ 
        errno = ENOENT;
    
  
  
    
        /* Error, entry exists but not a directory. */ 
        errno = ENOTDIR;
    
  
  
      /* Make an absolute pathname.  */ 
      _tfullpath (szFullPath, szPath, MAX_PATH);
  
      /* Allocate enough space to store DIR structure and the complete
       * directory path given. */ 
      nd =
      (_TDIR *) malloc (sizeof (_TDIR) + (_tcslen (szFullPath) +
          
  
    
        /* Error, out of memory. */ 
        errno = ENOMEM;
    
  
  
      /* Create the search expression. */ 
      _tcscpy (nd->dd_name, szFullPath);
  
      /* Add on a slash if the path does not end with one. */ 
      if (nd->dd_name[0] != _T ('\0')
      && 
      && 
    
  
  
      /* Add on the search pattern */ 
      _tcscat (nd->dd_name, SUFFIX);
  
      /* Initialize handle to -1 so that a premature closedir doesn't try
       * to call _findclose on it. */ 
      nd->dd_handle = -1;
  
      /* Initialize the status. */ 
      nd->dd_stat = 0;
  
      /* Initialize the dirent structure. ino and reclen are invalid under
       * Win32, and name simply points at the appropriate part of the
       * findfirst_t structure. */ 
      nd->dd_dir.d_ino = 0;
  
  
  
      // Added by jcsston 02/04/2004, memset was writing to a bad pointer
      nd->dd_dir.d_name = malloc (FILENAME_MAX);
  
      // End add
      memset (nd->dd_dir.d_name, 0, FILENAME_MAX);
  



/*
 * readdir
 *
 * Return a pointer to a dirent structure filled with the information on the
 * next entry in the directory.
 */ 
struct _tdirent *
_treaddir (_TDIR * dirp) 
{
  
  
      /* Check for valid DIR struct. */ 
      if (!dirp) {
    
    
  
  
    
        /* We have already returned all files in the directory
         * (or the structure has an invalid dd_stat). */ 
        return (struct _tdirent *) 0;
  
    
        /* We haven't started the search yet. */ 
        /* Start the search */ 
        dirp->dd_handle = _tfindfirst (dirp->dd_name, &(dirp->dd_dta));
    
      
          /* Whoops! Seems there are no files in that
           * directory. */ 
          dirp->dd_stat = -1;
    
      
    
  
    
        /* Get the next search entry. */ 
        if (_tfindnext (dirp->dd_handle, &(dirp->dd_dta))) {
      
          /* We are off the end or otherwise error.     
             _findnext sets errno to ENOENT if no more file
             Undo this. */ 
          DWORD winerr = GetLastError ();
      
        
      
      
      
    
      
          /* Update the status to indicate the correct
           * number. */ 
          dirp->dd_stat++;
    
  
  
    
        /* Successfully got an entry. Everything about the file is
         * already appropriately filled in except the length of the
         * file name. */ 
        dirp->dd_dir.d_namlen = _tcslen (dirp->dd_dta.name);
    
    
  
  


/*
 * closedir
 *
 * Frees up resources allocated by opendir.
 */ 
int 
_tclosedir (_TDIR * dirp) 
{
  

  
  
  
    
    
  
  
    
  
  
    
  
      /* Delete the dir structure. */ 
      free (dirp);
  



/*
 * rewinddir
 *
 * Return to the beginning of the directory "stream". We simply call findclose
 * and then reset things like an opendir.
 */ 
void 
_trewinddir (_TDIR * dirp) 
{
  
  
    
    
  
  
    
  
  
  



/*
 * telldir
 *
 * Returns the "position" in the "directory stream" which can be used with
 * seekdir to go back to an old entry. We simply return the value in stat.
 */ 
long 
_ttelldir (_TDIR * dirp) 
{
  
  
    
    
  
  



/*
 * seekdir
 *
 * Seek to an entry previously returned by telldir. We rewind the directory
 * and call readdir repeatedly until either dd_stat is the position number
 * or -1 (off the end). This is not perfect, in that the directory may
 * have changed while we weren't looking. But that is probably the case with
 * any such system.
 */ 
void 
_tseekdir (_TDIR * dirp, long lPos) 
{
  
  
    
    
  
  
    
        /* Seeking to an invalid position. */ 
        errno = EINVAL;
    
  
    
        /* Seek past end. */ 
        if (dirp->dd_handle != -1) {
      
    
    
    
  
    
        /* Rewind and read forward to the appropriate index. */ 
        _trewinddir (dirp);
    
  
