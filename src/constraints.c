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
/* File: constraints.c                                                       */
/*                                                                           */
/* Created: Wed Oct 17 13:00:08 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes)

/* Note rval must be pre-allocated for this function, e.g. use
   CopyRvalItem in call.  This is to make the parser and var expansion
   non-leaky */
    
{ struct Constraint *cp,*lp;
  char *sp = NULL;

switch(type)
   {
   case CF_SCALAR:
       Debug("   Appending Constraint: %s => %s\n",lval,rval);
       break;
   case CF_FNCALL:
       Debug("   Appending a function call to rhs\n");
       break;
   case CF_LIST:
       Debug("   Appending a list to rhs\n");
   }

if ((cp = (struct Constraint *)malloc(sizeof(struct Constraint))) == NULL)
   {
   CfLog(cferror,"Unable to allocate Constraint","malloc");
   FatalError("");
   }

if ((sp = strdup(lval)) == NULL)
   {
   CfLog(cferror,"Unable to allocate Constraint lval","malloc");
   FatalError("");
   }

if (*conlist == NULL)
   {
   *conlist = cp;
   }
else
   {
   for (lp = *conlist; lp->next != NULL; lp=lp->next)
      {
      }

   lp->next = cp;
   }

if (classes != NULL)
   {
   if ((cp->classes = strdup(classes)) == NULL)
      {
      CfLog(cferror,"Unable to allocate Constraint classes","malloc");
      FatalError("");
      }
   }
else
   {
   cp->classes = NULL;
   }

cp->audit = AUDITPTR;
cp->lineno = P.line_no;
cp->lval = sp;
cp->rval = rval;
cp->type = type;  /* literal, bodyname, builtin function */
cp->next = NULL;
return cp;
}

/*****************************************************************************/

void DeleteConstraintList(struct Constraint *conlist)

{ struct Constraint *cp, *next;

Debug("DeleteConstraintList()\n");
 
for (cp = conlist; cp != NULL; cp = next)
   {
   Debug("Delete lval = %s,%c\n",cp->lval,cp->type);
   
   next = cp->next;

   if (cp->rval)
      {
      DeleteRvalItem(cp->rval,cp->type);
      }

   if (cp->lval)
      {
      free((char *)cp->lval);
      }
   
   if (cp->classes)
      {
      free(cp->classes);
      }

   free((char *)cp);
   }
}

/*****************************************************************************/

int GetBooleanConstraint(char *lval,struct Constraint *list)

{ struct Constraint *cp;
  int retval = CF_UNDEFINED;

for (cp = list; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (strcmp(cp->rval,"true") == 0)
         {
         if (IsDefinedClass(cp->classes))
            {
            if (retval != CF_UNDEFINED)
               {
               snprintf(OUTPUT,CF_BUFSIZE,"Multiple %s constraints break this promise\n",lval);
               CfLog(cferror,OUTPUT,"");
               }
            retval = true;
            }
         }
      
      if (strcmp(cp->rval,"false") == 0)
         {
         if (IsDefinedClass(cp->classes))
            {
            if (retval != CF_UNDEFINED)
               {
               snprintf(OUTPUT,CF_BUFSIZE,"Multiple %s constraints break this promise\n",lval);
               CfLog(cferror,OUTPUT,"");
               }
            retval = false;
            }
         }
      }
   }

if (retval == CF_UNDEFINED)
   {
   retval = false;
   }

return retval;
}

/*****************************************************************************/

void *GetConstraint(char *lval,struct Constraint *list)

{ struct Constraint *cp;
  void *retval = NULL;

for (cp = list; cp != NULL; cp=cp->next)
   {
   if (strcmp(cp->lval,lval) == 0)
      {
      if (IsDefinedClass(cp->classes))
         {
         if (retval != NULL)
            {
            snprintf(OUTPUT,CF_BUFSIZE,"Inconsistent %s constraints break this promise\n",lval);
            CfLog(cferror,OUTPUT,"");
            }
         retval = cp->rval;
         }
      }
   }

return retval;
}

/*****************************************************************************/

void ReCheckAllConstraints(struct Promise *pp)

{ struct Constraint *cp;

 for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
    PostCheckConstraint(pp->agentsubtype,pp->bundle,cp->lval,cp->rval,cp->type);
    }     
}

/*****************************************************************************/

void PostCheckConstraint(char *type,char *bundle,char *lval,void *rval,char rvaltype)

{ struct SubTypeSyntax ss;
  int lmatch = false;
  int i,j,k,l;
  struct BodySyntax *bs,*bs2;
  struct SubTypeSyntax *ssp;

Debug("PostCheckConstraint(%s,%s)\n",type,lval);

for  (i = 0; i < CF3_MODULES; i++)
   {
   if ((ssp = CF_ALL_SUBTYPES[i]) == NULL)
      {
      continue;
      }
   
   for (j = 0; ssp[j].btype != NULL; j++)
      {
      ss = ssp[j];
      
      if (ss.subtype != NULL) 
         {
         if (strcmp(ss.subtype,type) == 0)
            {
            Debug("Found type %s's body syntax\n",type);
            
            bs = ss.bs;
            
            for (l = 0; bs[l].lval != NULL; l++)
               {
               if (strcmp(lval,bs[l].lval) == 0)
                  {
                  lmatch = true;
                  Debug("Postmatched syntatically correct bundle (lval,rval) item = (%s) to its rval\n",lval);
                  
                  if (bs[l].dtype == cf_body)
                     {
                     bs2 = (struct BodySyntax *)bs[l].range;
                     
                     for (i = 0; bs2[i].lval != NULL; i++)
                        {
                        if (strcmp(lval,bs2[i].lval) == 0)
                           {
                           CheckConstraintTypeMatch(lval,rval,rvaltype,bs2[i].dtype,(char *)(bs2[i].range));
                           return;
                           }
                        }
                     return;
                     }
                  else
                     {
                     CheckConstraintTypeMatch(lval,rval,rvaltype,bs[l].dtype,(char *)(bs[l].range));
                     return;
                     }
                  }
               }                        
            }
         }
      }
   }


/* Now check the functional modules - extra level of indirection */

for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
   {
   Debug1("CMP-common # %s,%s\n",lval,CF_COMMON_BODIES[i].lval);
   
   if (strcmp(lval,CF_COMMON_BODIES[i].lval) == 0)
      {
      Debug("Found a match for lval %s in the common constraint attributes\n",lval);
      CheckConstraintTypeMatch(lval,rval,rvaltype,CF_COMMON_BODIES[i].dtype,(char *)(CF_COMMON_BODIES[i].range));
      return;
      }
   }
}

