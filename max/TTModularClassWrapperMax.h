/* 
 *	TTModularClassWrapperMax
 *	An automated class wrapper to make TTBlue object's available as objects for Max/MSP
 *	Copyright © 2008 by Timothy Place
 * 
 * License: This code is licensed under the terms of the GNU LGPL
 * http://www.gnu.org/licenses/lgpl.html 
 */

#ifndef __TT_MODULAR_CLASS_WRAPPER_MAX_H__
#define __TT_MODULAR_CLASS_WRAPPER_MAX_H__

#include "ext.h"					// Max Header
#include "ext_obex.h"				// Max Object Extensions (attributes) Header
#include "ext_user.h"
#include "ext_common.h"
#include "TTClassWrapperMax.h"
#include "ext_hashtab.h"
#include "TTModular.h"				// Jamoma Modular API
#include "Nodelib.h"				// Jamoma Nodelib for Max (maybe it have to be renamed now ... ?)

// Method definition for specific TT class things
typedef void (*WrapTTModularClassSpecificities)(WrappedClassPtr c);


// Data Structure for this object
typedef struct _wrappedModularInstance {
    t_object								obj;						///< Max control object header
	TTHandle								outlets;					///< an array of outlet
	
	WrappedClassPtr							wrappedClassDefinition;		///< A pointer to the class definition
	TTObjectPtr								wrappedObject;				///< The instance of the TTBlue object we are wrapping
	TTSubscriberPtr							subscriberObject;			///< The instance of a TTSubscriber object used to 
																		///< register the wrapped object in the tree structure
} WrappedModularInstance;

typedef WrappedModularInstance* WrappedModularInstancePtr;	///< Pointer to a wrapped instance of our object.




// FUNCTIONS

// private:
// self has to be TTPtr because different wrappers (such as the ui wrapper) have different implementations.

ObjectPtr	wrappedModularClass_new(SymbolPtr name, AtomCount argc, AtomPtr argv);
void		wrappedModularClass_free(WrappedModularInstancePtr x);

t_max_err	wrappedModularClass_attrGet(TTPtr self, ObjectPtr attr, AtomCount* argc, AtomPtr* argv);
t_max_err	wrappedModularClass_attrSet(TTPtr self, ObjectPtr attr, AtomCount argc, AtomPtr argv);

void		wrappedModularClass_anything(TTPtr self, SymbolPtr s, AtomCount argc, AtomPtr argv);


// public:
// Wrap a TTBlue class as a Max class.
TTErr		wrapTTModularClassAsMaxClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, WrapTTModularClassSpecificities wrapTTClassSpec, WrappedClass_newSpecificities wrap_newSpec);

#endif // __TT_MODULAR_CLASS_WRAPPER_MAX_H__
