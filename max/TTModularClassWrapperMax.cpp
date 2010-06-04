/* 
 *	TTModularClassWrapperMax
 *	An automated class wrapper to make TTBlue object's available as objects for Max/MSP
 *	Copyright © 2008 by Timothy Place
 * 
 * License: This code is licensed under the terms of the GNU LGPL
 * http://www.gnu.org/licenses/lgpl.html 
 */

#include "TTModularClassWrapperMax.h"
#include "ext_hashtab.h"


/** A hash of all wrapped clases, keyed on the Max class name. */
static t_hashtab*	wrappedMaxClasses = NULL;


ObjectPtr wrappedModularClass_new(SymbolPtr name, AtomCount argc, AtomPtr argv)
{	
	WrappedClass*				wrappedMaxClass = NULL;
    WrappedModularInstancePtr	x = NULL;
	TTErr						err = kTTErrNone;
	
	// Find the WrappedClass
	hashtab_lookup(wrappedMaxClasses, name, (ObjectPtr*)&wrappedMaxClass);
	
	// If the WrappedClass has a validity check defined, then call the validity check function.
	// If it returns an error, then we won't instantiate the object.
	if(wrappedMaxClass){
		if(wrappedMaxClass->validityCheck)
			err = wrappedMaxClass->validityCheck(wrappedMaxClass->validityCheckArgument);
		else
			err = kTTErrNone;
	}
	else
		err = kTTErrGeneric;
	
	if(!err)
		x = (WrappedModularInstancePtr)object_alloc(wrappedMaxClass->maxClass);
	
    if(x){
		
		x->wrappedClassDefinition = wrappedMaxClass;
		
		x->subscriberObject = NULL;
		
		// Make specific things
		ModularSpec *spec = (ModularSpec*)wrappedMaxClass->specificities;
		if (spec)
			if (spec->_new)
				spec->_new((TTPtr)x, argc, argv);
		
		// handle attribute args
		attr_args_process(x, argc, argv);
	
	}
	return ObjectPtr(x);
}

void wrappedModularClass_free(WrappedModularInstancePtr x)
{
	TTObjectRelease(&x->wrappedObject);
	
	if (x->subscriberObject)
		TTObjectRelease(TTObjectHandle(&x->subscriberObject));
}

t_max_err wrappedModularClass_attrGet(TTPtr self, ObjectPtr attr, AtomCount* argc, AtomPtr* argv)
{
	SymbolPtr	attrName = (SymbolPtr)object_method(attr, _sym_getname);
	TTValue		v;
	AtomCount	i;
	WrappedModularInstancePtr x = (WrappedModularInstancePtr)self;
	TTSymbolPtr	ttAttrName = NULL;
	MaxErr		err;
	
	err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, attrName, (ObjectPtr*)&ttAttrName);
	if (err)
		return err;
	
	x->wrappedObject->getAttributeValue(ttAttrName, v);
	
	*argc = v.getSize();
	if (!(*argv)) // otherwise use memory passed in
		*argv = (t_atom *)sysmem_newptr(sizeof(t_atom) * v.getSize());
	
	for (i=0; i<v.getSize(); i++) {
		if(v.getType(i) == kTypeFloat32 || v.getType(i) == kTypeFloat64){
			TTFloat64	value;
			v.get(i, value);
			atom_setfloat(*argv+i, value);
		}
		else if(v.getType(i) == kTypeSymbol){
			TTSymbolPtr	value = NULL;
			v.get(i, &value);
			atom_setsym(*argv+i, gensym((char*)value->getCString()));
		}
		else{	// assume int
			TTInt32		value;
			v.get(i, value);
			atom_setlong(*argv+i, value);
		}
	}	
	return MAX_ERR_NONE;
}

t_max_err wrappedModularClass_attrSet(TTPtr self, ObjectPtr attr, AtomCount argc, AtomPtr argv)
{
	WrappedModularInstancePtr x = (WrappedModularInstancePtr)self;
	
	if (argc && argv) {
		
		SymbolPtr	attrName = (SymbolPtr)object_method(attr, _sym_getname);
		TTValue		v;
		AtomCount	i;
		TTSymbolPtr	ttAttrName = NULL;
		MaxErr		err;
		
		err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, attrName, (ObjectPtr*)&ttAttrName);
		if (err)
			return err;
		
		v.setSize(argc);
		for (i=0; i<argc; i++) {
			if(atom_gettype(argv+i) == A_LONG)
				v.set(i, AtomGetInt(argv+i));
			else if(atom_gettype(argv+i) == A_FLOAT)
				v.set(i, atom_getfloat(argv+i));
			else if(atom_gettype(argv+i) == A_SYM)
				v.set(i, TT(atom_getsym(argv+i)->s_name));
			else
				object_error(ObjectPtr(x), "bad type for attribute setter");
		}
		
		x->wrappedObject->setAttributeValue(ttAttrName, v);
		return MAX_ERR_NONE;
	}
	return MAX_ERR_GENERIC;
}
	
void wrappedModularClass_anything(TTPtr self, SymbolPtr s, AtomCount argc, AtomPtr argv)
{
	WrappedModularInstancePtr	x = (WrappedModularInstancePtr)self;
	TTValue				v;
	TTSymbolPtr			ttName = NULL;
	MaxErr				err;
	
	err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, s, (ObjectPtr*)&ttName);
	if (err)
		return;

	if (argc && argv) {
		TTValue	v;
		
		v.setSize(argc);
		for (AtomCount i=0; i<argc; i++) {
			if (atom_gettype(argv+i) == A_LONG)
				v.set(i, AtomGetInt(argv+i));
			else if (atom_gettype(argv+i) == A_FLOAT)
				v.set(i, atom_getfloat(argv+i));
			else if (atom_gettype(argv+i) == A_SYM)
				v.set(i, TT(atom_getsym(argv+i)->s_name));
			else
				object_error(ObjectPtr(x), "bad type for message arg");
		}
		
		x->wrappedObject->sendMessage(ttName, v);
		
		// process the returned value for the dumpout outlet
		{
			AtomCount	ac = v.getSize();

			if (ac) {
				AtomPtr		av = (AtomPtr)malloc(sizeof(Atom) * ac);
				
				for (AtomCount i=0; i<ac; i++) {
					if (v.getType() == kTypeSymbol){
						TTSymbolPtr ttSym = NULL;
						v.get(i, &ttSym);
						atom_setsym(av+i, gensym((char*)ttSym->getCString()));
					}
					else if (v.getType() == kTypeFloat32 || v.getType() == kTypeFloat64) {
						TTFloat64 f = 0.0;
						v.get(i, f);
						atom_setfloat(av+i, f);
					}
					else {
						TTInt32 l = 0;
						v.get(i, l);
						atom_setfloat(av+i, l);
					}
				}
				//TODO return value out (self, s, ac, av);
			}
		}
	}
	else
		x->wrappedObject->sendMessage(ttName);
}

TTErr wrapTTModularClassAsMaxClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, ModularSpec* specificities)
{
	TTObject*		o = NULL;
	TTValue			v, args;
	WrappedClass*	wrappedMaxClass = NULL;
	TTSymbolPtr		name = NULL;
	TTCString		nameCString = NULL;
	SymbolPtr		nameMaxSymbol = NULL;
	TTUInt32		nameSize = 0;
	
	common_symbols_init();
	TTModularInit();
	
	if (!wrappedMaxClasses)
		wrappedMaxClasses = hashtab_new(0);
	
	wrappedMaxClass = new WrappedClass;
	wrappedMaxClass->maxClassName = gensym(maxClassName);
	wrappedMaxClass->maxClass = class_new(	maxClassName, 
											(method)wrappedModularClass_new, 
											(method)wrappedModularClass_free, 
											sizeof(WrappedModularInstance), 
											(method)0L, 
											A_GIMME, 
											0);
	wrappedMaxClass->ttblueClassName = ttblueClassName;
	wrappedMaxClass->validityCheck = NULL;
	wrappedMaxClass->validityCheckArgument = NULL;
	wrappedMaxClass->options = NULL;
	wrappedMaxClass->maxNamesToTTNames = hashtab_new(0);
	
	wrappedMaxClass->specificities = specificities;
	
	// Create a temporary instance of the class so that we can query it.
	TTObjectInstantiate(ttblueClassName, &o, args);

	// Register Messages as Max method
	o->getMessageNames(v);
	for (TTUInt16 i=0; i<v.getSize(); i++) {
		v.get(i, &name);
		nameSize = strlen(name->getCString());
		nameCString = new char[nameSize+1];
		strncpy_zero(nameCString, name->getCString(), nameSize+1);

		if (nameCString[0] > 64 && nameCString[0] < 91) {
			nameCString[0] += 32;												// convert first letter to lower-case for Max
			nameMaxSymbol = gensym(nameCString);
			
			hashtab_store(wrappedMaxClass->maxNamesToTTNames, nameMaxSymbol, ObjectPtr(name));
			class_addmethod(wrappedMaxClass->maxClass, (method)wrappedModularClass_anything, nameCString, A_GIMME, 0);
		}
		delete nameCString;
		nameCString = NULL;
	}
	
	// Register Attributes as Max attr
	o->getAttributeNames(v);
	for (TTUInt16 i=0; i<v.getSize(); i++) {
		TTAttributePtr	attr = NULL;
		SymbolPtr		maxType = _sym_long;
		
		v.get(i, &name);
		nameSize = strlen(name->getCString());
		nameCString = new char[nameSize+1];
		strncpy_zero(nameCString, name->getCString(), nameSize+1);

		// only expose messages to Max if they begin with an upper-case letter
		if (nameCString[0]>64 && nameCString[0]<91) {
			nameCString[0] += 32;
			nameMaxSymbol = gensym(nameCString);
					
			o->findAttribute(name, &attr);
			
			if (attr->type == kTypeFloat32)
				maxType = _sym_float32;
			else if (attr->type == kTypeFloat64)
				maxType = _sym_float64;
			else if (attr->type == kTypeSymbol || attr->type == kTypeString)
				maxType = _sym_symbol;
			
			hashtab_store(wrappedMaxClass->maxNamesToTTNames, nameMaxSymbol, ObjectPtr(name));
			class_addattr(wrappedMaxClass->maxClass, attr_offset_new(nameCString, maxType, 0, (method)wrappedModularClass_attrGet, (method)wrappedModularClass_attrSet, NULL));
			
			// Add display styles for the Max 5 inspector
			if (attr->type == kTypeBoolean)
				CLASS_ATTR_STYLE(wrappedMaxClass->maxClass, (char*)name->getCString(), 0, "onoff");
			if (name == TT("fontFace"))
				CLASS_ATTR_STYLE(wrappedMaxClass->maxClass,	"fontFace", 0, "font");
		}
		delete nameCString;
		nameCString = NULL;
	}
	
	TTObjectRelease(&o);
	
	class_addmethod(wrappedMaxClass->maxClass, (method)stdinletinfo,			"inletinfo",	A_CANT, 0);
	class_addmethod(wrappedMaxClass->maxClass, (method)specificities->_any,		"anything",		A_GIMME, 0);
	
	// Register specific methods and do specific things
	if (specificities)
		if (specificities->_wrap)
			specificities->_wrap(wrappedMaxClass);
	
	class_register(_sym_box, wrappedMaxClass->maxClass);
	if (c)
		*c = wrappedMaxClass;
	
	hashtab_store(wrappedMaxClasses, wrappedMaxClass->maxClassName, ObjectPtr(wrappedMaxClass));
	return kTTErrNone;
}