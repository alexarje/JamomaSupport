/* 
 *	TTClassWrapperMax
 *	An automated class wrapper to make TTBlue object's available as objects for Max/MSP
 *	Copyright © 2008 by Timothy Place
 * 
 * License: This code is licensed under the terms of the GNU LGPL
 * http://www.gnu.org/licenses/lgpl.html 
 */

#include "TTGraphicsClassWrapperMax.h"


// Data Structure for this object
typedef struct _wrappedUIInstance {
    t_jbox				box;						///< Max audio object header
	WrappedClassPtr		wrappedClassDefinition;		///< A pointer to the class definition
	TTAudioObjectPtr	wrappedObject;				///< The instance of the TTBlue object we are wrapping
} WrappedUIInstance;
typedef WrappedUIInstance* WrappedUIInstancePtr;	///< Pointer to a wrapped instance of our object.


/** A hash of all wrapped clases, keyed on the Max class name. */
static t_hashtab*	wrappedMaxClasses = NULL;


ObjectPtr wrappedUIClass_new(SymbolPtr name, AtomCount argc, AtomPtr argv)
{	
	WrappedClass*			wrappedMaxClass = NULL;
    WrappedUIInstancePtr	x = NULL;
	TTValue					v;
 	//long					attrstart = attr_args_offset(argc, argv);		// support normal arguments
	TTErr					err = kTTErrNone;
	t_dictionary*			d = NULL;
	long					flags;
	
	if (!(d=object_dictionaryarg(argc,argv)))
		return NULL;
	
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
		x = (WrappedUIInstancePtr)object_alloc(wrappedMaxClass->maxClass);
    if(x){
		x->wrappedClassDefinition = wrappedMaxClass;
		
		TTObjectInstantiate(wrappedMaxClass->ttblueClassName, &x->wrappedObject, kTTValNONE);

    	object_obex_store((void *)x, _sym_dumpout, (object *)outlet_new(x,NULL));	// dumpout		
		//for(short i=0; i < x->numOutputs; i++)
		//	outlet_new((t_pxobject *)x, "signal");			// outlets

		flags = 0 
		| JBOX_DRAWFIRSTIN		// 0
		| JBOX_NODRAWBOX		// 1
		| JBOX_DRAWINLAST		// 2
		//	| JBOX_TRANSPARENT		// 3
		//	| JBOX_NOGROW			// 4
		//	| JBOX_GROWY			// 5
		| JBOX_GROWBOTH			// 6
		//	| JBOX_IGNORELOCKCLICK	// 7
		//	| JBOX_HILITE			// 8
		//	| JBOX_BACKGROUND		// 9
		//	| JBOX_NOFLOATINSPECTOR	// 10
		//	| JBOX_TEXTFIELD		// 11
		| JBOX_MOUSEDRAGDELTA	// 12
		//	| JBOX_COLOR			// 13
		//	| JBOX_BINBUF			// 14
		//	| JBOX_DRAWIOLOCKED		// 15
		//	| JBOX_DRAWBACKGROUND	// 16
		//	| JBOX_NOINSPECTFIRSTIN	// 17
		//	| JBOX_DEFAULTNAMES		// 18
		//	| JBOX_FIXWIDTH			// 19
		;
		
		jbox_new(&x->box, flags, argc, argv);
		x->box.b_firstin = (t_object *)x;
		attr_dictionary_process(x,d);
		jbox_ready((t_jbox *)x);
	}
	return ObjectPtr(x);
}


void wrappedUIClass_free(WrappedUIInstancePtr x)
{
	notify_free((t_object *)x);
	jbox_free((t_jbox *)x);
	TTObjectRelease(&x->wrappedObject);
}


t_max_err wrappedClass_notify(WrappedUIInstancePtr x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{	
	if (msg == _sym_modified)
		jbox_redraw(&x->box);	
	else if ((msg == _sym_attr_modified) && (sender == x))	
		jbox_redraw(&x->box);
	
	return jbox_notify((t_jbox*)x, s, msg, sender, data);
}


void wrappedClass_paint(WrappedUIInstancePtr x, t_object *view)
{
	t_rect			rect;
	t_rect			r;
	t_jgraphics*	g;
	t_jsurface*		jsurface;
	unsigned char*	data;
	TTValue			v;
	TTErr			err;
	TTInt32			width;
	TTInt32			height;
	TTInt32			stride;
	
	g = (t_jgraphics*)patcherview_get_jgraphics(view);		// obtain graphics context
	jbox_get_rect_for_view((t_object *)x, view, &rect);		// this is the box rectangle -- but we draw relative to 0 0, and thus only care about width & height
	
	v.setSize(2);
	v.set(0, rect.width);
	v.set(1, rect.height);
	err = x->wrappedObject->sendMessage(TT("resize"), v);
	err = x->wrappedObject->sendMessage(TT("paint"));
	err = x->wrappedObject->sendMessage(TT("getData"), v);
	if (!err) {
		data = (unsigned char*)TTPtr(v);
		v.get(1, width);
		v.get(2, height);
		v.get(3, stride);
		
		jsurface = jgraphics_image_surface_create_for_data(data, JGRAPHICS_FORMAT_ARGB32, width, height, stride, NULL, NULL);
		
		r.x = 0;
		r.y = 0;
		r.width = rect.width;
		r.height = rect.height;
		jgraphics_image_surface_draw(g, jsurface, r, r);
		jgraphics_surface_destroy(jsurface);
	}
}


// For the following 3 methods, we can't just use the plain TTClassWrapper code 
// because the size of a t_jbox is different than the size of a t_pxobject.

t_max_err wrappedUIClass_attrGet(TTPtr self, ObjectPtr attr, AtomCount* argc, AtomPtr* argv)
{
	SymbolPtr	attrName = (SymbolPtr)object_method(attr, _sym_getname);
	TTValue		v;
	AtomCount	i;
	WrappedUIInstancePtr x = (WrappedUIInstancePtr)self;
	
	x->wrappedObject->getAttributeValue(TT(attrName->s_name), v);
	
	*argc = v.getSize();
	if (!(*argv)) // otherwise use memory passed in
		*argv = (t_atom *)sysmem_newptr(sizeof(t_atom) * v.getSize());
	
	for(i=0; i<v.getSize(); i++){
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


t_max_err wrappedUIClass_attrSet(TTPtr self, ObjectPtr attr, AtomCount argc, AtomPtr argv)
{
	WrappedUIInstancePtr x = (WrappedUIInstancePtr)self;
	
	if(argc && argv){
		SymbolPtr	attrName = (SymbolPtr)object_method(attr, _sym_getname);
		TTValue		v;
		AtomCount	i;
		
		v.setSize(argc);
		for(i=0; i<argc; i++){
			if(atom_gettype(argv+i) == A_LONG)
				v.set(i, AtomGetInt(argv+i));
			else if(atom_gettype(argv+i) == A_FLOAT)
				v.set(i, atom_getfloat(argv+i));
			else if(atom_gettype(argv+i) == A_SYM)
				v.set(i, TT(atom_getsym(argv+i)->s_name));
			else
				object_error(ObjectPtr(x), "bad type for attribute setter");
		}
		x->wrappedObject->setAttributeValue(TT(attrName->s_name), v);
		return MAX_ERR_NONE;
	}
	return MAX_ERR_GENERIC;
}


void wrappedUIClass_anything(TTPtr self, SymbolPtr s, AtomCount argc, AtomPtr argv)
{
	WrappedUIInstancePtr x = (WrappedUIInstancePtr)self;
	
	if(argc && argv){
		TTValue	v;
		
		v.setSize(argc);
		for(AtomCount i=0; i<argc; i++){
			if(atom_gettype(argv+i) == A_LONG)
				v.set(i, AtomGetInt(argv+i));
			else if(atom_gettype(argv+i) == A_FLOAT)
				v.set(i, atom_getfloat(argv+i));
			else if(atom_gettype(argv+i) == A_SYM)
				v.set(i, TT(atom_getsym(argv+i)->s_name));
			else
				object_error(ObjectPtr(x), "bad type for message arg");
		}
		x->wrappedObject->sendMessage(TT(s->s_name), v);
	}
	else
		x->wrappedObject->sendMessage(TT(s->s_name));
}


TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c)
{
	return wrapTTClassAsMaxUIClass(ttblueClassName, maxClassName, c, (WrappedClassOptionsPtr)NULL);
}

TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, WrappedClassOptionsPtr options)
{
	TTObject*		o = NULL;
	TTValue			v;
	WrappedClass*	wrappedMaxClass = NULL;
	long			flags = 0;

	common_symbols_init();
	TTDSPInit();
	
	if(!wrappedMaxClasses)
		wrappedMaxClasses = hashtab_new(0);
	
	wrappedMaxClass = new WrappedClass;
	wrappedMaxClass->maxClassName = gensym(maxClassName);
	wrappedMaxClass->maxClass = class_new(	maxClassName, 
											(method)wrappedUIClass_new, 
											(method)wrappedUIClass_free, 
											sizeof(WrappedUIInstance), 
											(method)0L, 
											A_GIMME, 
											0);
	wrappedMaxClass->ttblueClassName = ttblueClassName;
	wrappedMaxClass->validityCheck = NULL;
	wrappedMaxClass->validityCheckArgument = NULL;
	wrappedMaxClass->options = options;
	
	jbox_initclass(wrappedMaxClass->maxClass, flags);	
	wrappedMaxClass->maxClass->c_flags |= CLASS_FLAG_NEWDICTIONARY; // to specify dictionary constructor
	
	// Create a temporary instance of the class so that we can query it.
	TTObjectInstantiate(ttblueClassName, &o, kTTValNONE);

	o->getMessageNames(v);
	for (TTUInt16 i=0; i<v.getSize(); i++) {
		TTSymbolPtr			name = NULL;
		
		v.get(i, &name);
		if (name == TT("updateMaxNumChannels") || name == TT("updateSr"))
			continue;	// don't expose these attributes to Max users

		class_addmethod(wrappedMaxClass->maxClass, (method)wrappedUIClass_anything, (char*)name->getCString(), A_GIMME, 0);
	}
	
	o->getAttributeNames(v);
	for (TTUInt16 i=0; i<v.getSize(); i++) {
		TTSymbolPtr		name = NULL;
		TTAttributePtr	attr = NULL;
		t_symbol*		maxType = _sym_long;
		
		v.get(i, &name);
		if(name == TT("maxNumChannels") || name == TT("processInPlace"))
			continue;	// don't expose these attributes to Max users
		
		o->findAttribute(name, &attr);

		if(attr->type == kTypeFloat32)
			maxType = _sym_float32;
		else if(attr->type == kTypeFloat64)
			maxType = _sym_float64;
		else if(attr->type == kTypeSymbol || attr->type == kTypeString)
			maxType = _sym_symbol;
		
		class_addattr(wrappedMaxClass->maxClass, attr_offset_new((char*)name->getCString(), maxType, 0, (method)wrappedUIClass_attrGet, (method)wrappedUIClass_attrSet, NULL));
		CLASS_ATTR_SAVE(wrappedMaxClass->maxClass, (char*)name->getCString(), 0);
		
		// Add display styles for the Max 5 inspector
		if (attr->type == kTypeBoolean)
			CLASS_ATTR_STYLE(wrappedMaxClass->maxClass, (char*)name->getCString(), 0, "onoff");
		if (name == TT("FontFace"))
			CLASS_ATTR_STYLE(wrappedMaxClass->maxClass,	"FontFace", 0, "font");
	}
	
	TTObjectRelease(&o);
		
 	class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_paint, 		"paint",		A_CANT, 0L);
 	class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_notify, 	"notify",		A_CANT, 0L);
    class_addmethod(wrappedMaxClass->maxClass, (method)object_obex_dumpout, 	"dumpout",		A_CANT, 0); 
	class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_assist, 	"assist",		A_CANT, 0L);
	class_addmethod(wrappedMaxClass->maxClass, (method)stdinletinfo,			"inletinfo",	A_CANT, 0);
	
	CLASS_ATTR_DEFAULT(wrappedMaxClass->maxClass,	"patching_rect",	0,	"0. 0. 160. 160.");
	CLASS_ATTR_MIN(wrappedMaxClass->maxClass,		"patching_size",	0,	"1. 1.");

	class_register(_sym_box, wrappedMaxClass->maxClass);
	if (c)
		*c = wrappedMaxClass;
	
	hashtab_store(wrappedMaxClasses, wrappedMaxClass->maxClassName, ObjectPtr(wrappedMaxClass));
	return kTTErrNone;
}


TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck)
{
	TTErr err = wrapTTClassAsMaxUIClass(ttblueClassName, maxClassName, c);
	
	if(!err){
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = (*c)->maxClass;
	}
	return err;
}

TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, WrappedClassOptionsPtr options)
{
	TTErr err = wrapTTClassAsMaxUIClass(ttblueClassName, maxClassName, c, options);
	
	if(!err){
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = (*c)->maxClass;
	}
	return err;
}


TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument)
{
	TTErr err = wrapTTClassAsMaxUIClass(ttblueClassName, maxClassName, c);
	
	if(!err){
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = validityCheckArgument;
	}
	return err;
}

TTErr wrapTTClassAsMaxUIClass(TTSymbolPtr ttblueClassName, char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument, WrappedClassOptionsPtr options)
{
	TTErr err = wrapTTClassAsMaxUIClass(ttblueClassName, maxClassName, c, options);
	
	if(!err){
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = validityCheckArgument;
	}
	return err;
}

