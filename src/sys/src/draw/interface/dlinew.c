#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: dlinew.c,v 1.21 1999/10/13 20:36:30 bsmith Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "src/sys/src/draw/drawimpl.h"  /*I "draw.h" I*/

#undef __FUNC__  
#define __FUNC__ "DrawLineSetWidth" 
/*@
   DrawLineSetWidth - Sets the line width for future draws.  The width is
   relative to the user coordinates of the window; 0.0 denotes the natural
   width; 1.0 denotes the entire viewport. 

   Not collective

   Input Parameters:
+  draw - the drawing context
-  width - the width in user coordinates

   Level: advanced

.keywords:  draw, line, set, width

.seealso:  DrawLineGetWidth()
@*/
int DrawLineSetWidth(Draw draw,double width)
{
  int        ierr;
  PetscTruth isdrawnull;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  ierr = PetscTypeCompare((PetscObject)draw,DRAW_NULL,&isdrawnull);CHKERRQ(ierr);
  if (isdrawnull) PetscFunctionReturn(0);
  ierr = (*draw->ops->linesetwidth)(draw,width);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
