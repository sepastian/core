/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: files_links.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

int VerifyLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char to[CF_BUFSIZE],linkbuf[CF_BUFSIZE],saved[CF_BUFSIZE],absto[CF_BUFSIZE];
  int nofile = false;
  struct stat sb;
      
Debug("Linkfiles(%s -> %s)\n",destination,source);

memset(to,0,CF_BUFSIZE);
  
if ((*source != '/') && (*source != '.'))  /* links without a directory reference */
   {
   snprintf(to,CF_BUFSIZE-1,"./%s",source);
   }
else
   {
   strncpy(to,source,CF_BUFSIZE-1);
   }

if (*to != '/')         /* relative path, must still check if exists */
   {
   Debug("Relative link destination detected: %s\n",to);
   strcpy(absto,AbsLinkPath(destination,to));
   Debug("Absolute path to relative link = %s, destination %s\n",absto,destination);
   }
else
   {
   strcpy(absto,to);
   }

if (stat(absto,&sb) == -1)
   {
   Debug("No source file\n");
   nofile = true;
   }

if (nofile && (attr.link.when_no_file != cfa_force) && (attr.link.when_no_file != cfa_delete))
   {
   CfOut(cf_inform,"","Source %s for linking is absent",absto);
   return false;
   }

if (nofile && attr.link.when_no_file == cfa_delete)
   {
   KillGhostLink(destination,attr,pp);
   return true;
   }
    
memset(linkbuf,0,CF_BUFSIZE);

if (readlink(destination,linkbuf,CF_BUFSIZE-1) == -1)
   {
   if (!MakeParentDirectory(destination,attr.move_obstructions))                  /* link doesn't exist */
      {
      return true;
      }
   else
      {
      if (!MoveObstruction(destination,attr,pp))
         {
         return false;
         }

      return MakeLink(destination,to,attr,pp);
      }
   }
else
   { int off1 = 0, off2 = 0;  /* Link exists */
   
   DeleteSlash(linkbuf);
   
   if (strncmp(linkbuf,"./",2) == 0)   /* Ignore ./ at beginning */
      {
      off1 = 2;
      }
   
   if (strncmp(to,"./",2) == 0)
      {
      off2 = 2;
      }
   
   if (strcmp(linkbuf+off1,to+off2) != 0)
      {
      if (attr.move_obstructions)
         {
         if (!DONTDO)
            {
            cfPS(cf_inform,CF_CHG,"",pp,attr,"Overriding incorrect link %s\n",destination);
            
            if (unlink(destination) == -1)
               {
               perror("unlink");
               return true;
               }
            
            return MakeLink(destination,to,attr,pp);
            }
         else
            {
            CfOut(cf_error,"","Must remove incorrect link %s\n",destination);
            return false;
            }
         }
      else
         {
         cfPS(cf_inform,CF_FAIL,"",pp,attr,"Link %s points to %s not %s - not authorized to override",destination,linkbuf,to);
         return true;
         }
      }
   else
      {
      cfPS(cf_inform,CF_NOP,"",pp,attr,"Link %s points to %s - promise kept",destination,to);
      return true;
      }
   }
}

/*****************************************************************************/

int VerifyAbsoluteLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char absto[CF_BUFSIZE];
  char expand[CF_BUFSIZE];
  char linkto[CF_BUFSIZE];
  
Debug("VerifyAbsoluteLink(%s,%s)\n",destination,source);

if (*source == '.')
   {
   strcpy(linkto,destination);
   ChopLastNode(linkto);
   AddSlash(linkto);
   strcat(linkto,source);
   }
else
   {
   strcpy(linkto,source);
   }

CompressPath(absto,linkto);

expand[0] = '\0';

if (attr.link.when_no_file == cfa_force)
   {  
   if (!ExpandLinks(expand,absto,0))  /* begin at level 1 and beam out at 15 */
      {
      CfOut(cf_error,"","Failed to make absolute link in\n");
      PromiseRef(cf_error,pp);
      return false;
      }
   else
      {
      Debug2("ExpandLinks returned %s\n",expand);
      }
   }
else
   {
   strcpy(expand,absto);
   }

CompressPath(linkto,expand);

return VerifyLink(destination,linkto,attr,pp);
}

/*****************************************************************************/

int VerifyRelativeLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char *sp, *commonto, *commonfrom;
  char buff[CF_BUFSIZE],linkto[CF_BUFSIZE];
  int levels=0;
  
Debug("RelativeLink(%s,%s)\n",destination,source);

if (*source == '.')
   {
   return VerifyLink(destination,source,attr,pp);
   }

if (!CompressPath(linkto,source))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,attr,"Failed to link %s to %s\n",destination,source);
   return false;
   }

commonto = linkto;
commonfrom = destination;

if (strcmp(commonto,commonfrom) == 0)
   {
   CfOut(cf_error,"","Can't link file to itself!\n");
   PromiseRef(cf_error,pp);
   return false;
   }

while (*commonto == *commonfrom)
   {
   commonto++;
   commonfrom++;
   }

while (!((*commonto == '/') && (*commonfrom == '/')))
   {
   commonto--;
   commonfrom--;
   }

commonto++; 

for (sp = commonfrom; *sp != '\0'; sp++)
   {
   if (*sp == '/')
       {
       levels++;
       }
   }

memset(buff,0,CF_BUFSIZE);

strcat(buff,"./");

while(--levels > 0)
   {
   if (BufferOverflow(buff,"../"))
      {
      return false;
      }
   
   strcat(buff,"../");
   }

if (BufferOverflow(buff,commonto))
   {
   return false;
   }

strcat(buff,commonto);
 
return VerifyLink(destination,buff,attr,pp);
}

/*****************************************************************************/

int VerifyHardLink(char *destination,char *source,struct Attributes attr,struct Promise *pp)

{ char to[CF_BUFSIZE],linkbuf[CF_BUFSIZE],saved[CF_BUFSIZE],absto[CF_BUFSIZE];
 struct stat ssb,dsb;

memset(to,0,CF_BUFSIZE);
  
if ((*source != '/') && (*source != '.'))  /* links without a directory reference */
   {
   snprintf(to,CF_BUFSIZE-1,"./%s",source);
   }
else
   {
   strncpy(to,source,CF_BUFSIZE-1);
   }

if (*to != '/')         /* relative path, must still check if exists */
   {
   Debug("Relative link destination detected: %s\n",to);
   strcpy(absto,AbsLinkPath(destination,to));
   Debug("Absolute path to relative link = %s, destination %s\n",absto,destination);
   }
else
   {
   strcpy(absto,to);
   }

if (stat(absto,&ssb) == -1)
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,attr,"Source file %s doesn't exist\n",source);
   return false;
   }

if (!S_ISREG(ssb.st_mode))
   {
   cfPS(cf_inform,CF_FAIL,"",pp,attr,"Source file %s is not a regular file, not appropriate to hard-link\n",to);
   return false;
   }

Debug2("Trying to (hard) link %s -> %s\n",destination,to);

if (stat(destination,&dsb) == -1)
   {
   MakeHardLink(destination,to,attr,pp);
   return true;
   }

 /* both files exist, but are they the same file? POSIX says  */
 /* the files could be on different devices, but unix doesn't */
 /* allow this behaviour so the tests below are theoretical...*/

if (dsb.st_ino != ssb.st_ino && dsb.st_dev != ssb.st_dev)
   {
   Verbose("If this is POSIX, unable to determine if %s is hard link is correct\n",destination);
   Verbose("since it points to a different filesystem!\n");

   if (dsb.st_mode == ssb.st_mode && dsb.st_size == ssb.st_size)
      {
      cfPS(cf_verbose,CF_NOP,"",pp,attr,"Hard link (%s->%s) on different device APPEARS okay\n",destination,to);
      return true;
      }
   }

if (dsb.st_ino == ssb.st_ino && dsb.st_dev == ssb.st_dev)
   {
   cfPS(cf_verbose,CF_NOP,"",pp,attr,"Hard link (%s->%s) exists and is okay\n",destination,to);
   return true;
   }

CfOut(cf_inform,"","%s does not appear to be a hard link to %s\n",destination,to);

if (!MoveObstruction(destination,attr,pp))
   {
   return false;
   }

MakeHardLink(destination,to,attr,pp);
return true;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int KillGhostLink(char *name,struct Attributes attr,struct Promise *pp)

{ char linkbuf[CF_BUFSIZE],tmp[CF_BUFSIZE];
  char linkpath[CF_BUFSIZE],*sp;
  struct stat statbuf;

Debug("KillGhostLink(%s)\n",name);

memset(linkbuf,0,CF_BUFSIZE);
memset(linkpath,0,CF_BUFSIZE); 

if (readlink(name,linkbuf,CF_BUFSIZE-1) == -1)
   {
   CfOut(cf_verbose,""," !! (Can't read link %s while checking for deadlinks)\n",name);
   return true; /* ignore */
   }

if (linkbuf[0] != '/')
   {
   strcpy(linkpath,name);    /* Get path to link */

   for (sp = linkpath+strlen(linkpath); (*sp != '/') && (sp >= linkpath); sp-- )
     {
     *sp = '\0';
     }
   }

strcat(linkpath,linkbuf);
CompressPath(tmp,linkpath); 
 
if (stat(tmp,&statbuf) == -1)               /* link points nowhere */
   {
   if (attr.link.when_no_file == cfa_delete || attr.recursion.rmdeadlinks)
      {
      CfOut(cf_verbose,"","%s is a link which points to %s, but that file doesn't seem to exist\n",name,VBUFF);

      if (!DONTDO)
         {
         unlink(name);  /* May not work on a client-mounted system ! */
         cfPS(cf_inform,CF_CHG,"",pp,attr,"Removing ghost %s - reference to something that is not there\n",name);
         return true;
         }
      }
   }

return false;
}

/*****************************************************************************/

int MakeLink (char *from,char *to,struct Attributes attr,struct Promise *pp)

{
if (DONTDO)
   {
   CfOut(cf_error,"","Need to link files %s -> %s\n",from,to);
   return false;
   }
else
   {
   if (symlink(to,from) == -1)
      {
      cfPS(cf_error,CF_FAIL,"symlink",pp,attr,"Couldn't link %s to %s\n",to,from);
      return false;
      }
   else
      {
      cfPS(cf_inform,CF_CHG,"",pp,attr,"Linked files %s -> %s\n",from,to);
      return true;
      }
   }
}

/*****************************************************************************/

int MakeHardLink (char *from,char *to,struct Attributes attr,struct Promise *pp)

{
if (DONTDO)
   {
   CfOut(cf_error,"","Need to hard link files %s -> %s\n",from,to);
   return false;
   }
else
   {
   if (link(to,from) == -1)
      {
      cfPS(cf_error,CF_FAIL,"link",pp,attr,"Couldn't (hard) link %s to %s\n",to,from);
      return false;
      }
   else
      {
      cfPS(cf_inform,CF_CHG,"",pp,attr,"(Hard) Linked files %s -> %s\n",from,to);
      return true;
      }
   }
}
