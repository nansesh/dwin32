#pragma once

#include "SOperand.h"
#include <vector>
#include <list>

template<class T>
class CSimpleObjCont
{
public:
	///**
	//* @brief		construct this container using a pointer to the contained object.
	//*				will take the ownership of the object pointed by pObj.
	//* @param		pObj [in] pointer to existing object.
	//*/
	explicit CSimpleObjCont(T* pObj = 0):m_pObj(pObj)
	{}

	///**
	//* @brief		creates the contained object from an exisiting one.
	//*				does not take ownership of the existing one.
	//*/
	explicit CSimpleObjCont(const T& o):m_pObj(0)
	{
		m_pObj = new T(o);
	}

	///**
	//* @brief	copy c'tor
	//* @param	o [in]
	//*/
	CSimpleObjCont(const CSimpleObjCont& o):m_pObj(0)
	{
		*this = o;
	}

	~CSimpleObjCont()
	{
		delete m_pObj;
	}

	///**
	//* @brief		makes deep copy of a existing object.
	//* @param		o [in] existing object.
	//* @return		reference to 'this' object
	//*/
	CSimpleObjCont& operator = (const CSimpleObjCont& o)
	{
		if(this==&o) return *this;

		delete m_pObj;
		m_pObj = (o.m_pObj?new T(*(o.m_pObj)):0);

		return *this;
	}

	///**
	//* @brief		returns the pointer to the contained object.
	//*	@return		pointer to the object.		
	//*/
	T* operator -> ()
	{
		return m_pObj;
	}

	///**
	//* @brief		returns the pointer to the contained object.
	//*	@return		pointer to the object.		
	//*/
	const T* operator -> () const
	{
		return m_pObj;
	}

	///**
	//* @brief		returns the pointer to the contained object.
	//*	@return		pointer to the object.		
	//*/
	T* Get()
	{
		return m_pObj;
	}

	///**
	//* @brief		returns the pointer to the contained object.
	//*	@return		pointer to the object.		
	//*/
	const T* Get() const 
	{
		return m_pObj;
	}

	///**
	//* @brief		Use to take ownership of a different object. Releases the 
	//*				contained object.
	//*	@return		reference to pointer that points to the object (T type) this 
	//*				object contains, use the reference to the pointer to reassign 
	//*				it to make it point to another object.
	//*
	//*/
	T*& GetObjPtrRef()
	{
		delete m_pObj;
		m_pObj = 0;

		return m_pObj;
	}

	///**
	//* @brief		Releases ownership on the contained object.
	//*				
	//*	@return		pointer to the previously contained object.
	//*
	//*/
	T* Release()
	{
		T* pRet = m_pObj;
		m_pObj = 0;
		return pRet;
	}

private:
	T* m_pObj;
};

class FunctionType;
class SmType
{
public:
	SmType()
		:m_t(Int)
		,m_mod(Modifier_None)
		,m_p(0)
	{}

	FunctionType* GetFT() const
	{
		return m_p;
	}

	void SetFT(FunctionType* p)
	{
		m_t = ObjRefType;
		m_p = p;
	}

	Types m_t;
	TypeModifiers m_mod;
	FunctionType* m_p;
};

template<class T>
void ResetValue(T& t);

template<>
inline void ResetValue<bool>(bool& b)
{
	b = false;
}

template<class T>
inline T ConsumeAndReset(T& t,void (*resetter) (T&) = &ResetValue<T>)
{
	T copy = t;
	(*resetter)(t);
	return copy;
}

class TypeModifiersMgr
{
public:
	explicit TypeModifiersMgr(bool& bRef)
		:m_bRef(bRef)
		,m_bRefCopy(bRef)
	{}

	void Consume()
	{
		m_bRefCopy = ConsumeAndReset(m_bRef);
	}

	bool ReferenceModifierValue()
	{
		return m_bRefCopy;
	}

private:
	bool& m_bRef;
	bool m_bRefCopy;
};

class FunctionType
{
public:
	FunctionType()
		:m_bReturnSpecified(false)
	{}

	~FunctionType()
	{
		Clear();
	}

public:
	static bool FunctionTypeFromSignature(const wchar_t* pSig,FunctionType& oType)
	{
		return FunctionTypeFromSignature(oType,pSig);
	}
private:
	//FunctionType(const FunctionType&);
	//FunctionType& operator = (const FunctionType&);

	static bool FunctionTypeFromSignature(FunctionType& oType,const wchar_t*& pSig)
	{
		if(!pSig || !(*pSig))
			return false;

		oType.Clear();
		
		bool bReferenceModifier = false;
		TypeModifiersMgr tModMgr(bReferenceModifier);

		bool bSetReturnType= false;
		bool bSettingReturnTypeDone = false;
		for(;*pSig;++pSig)
		{
			SmType t;
			if(*pSig==L'r')
			{
				bReferenceModifier = true;
				continue;
			}

			tModMgr.Consume();
			t.m_mod = tModMgr.ReferenceModifierValue()?Ref_Modifier:Modifier_None;

			switch(*pSig)
			{
			case L'i':
				if(bSetReturnType)
				{
					oType.m_returnType = t;
					bSettingReturnTypeDone = true;
				}
				else oType.m_oArgTypes.push_back(t);
				break;
			case L'v':
				if(bSetReturnType)
				{
					oType.m_returnType = t;
					bSettingReturnTypeDone = true;
				}
				else oType.m_oArgTypes.push_back(t);
				break;
			case L's':
				t.m_t = String;
				if(bSetReturnType)
				{
					oType.m_returnType = t;
					bSettingReturnTypeDone = true;
				}
				else oType.m_oArgTypes.push_back(t);
				break;
			case L')':
				{
					bSetReturnType = true;
				}
				break;
			case L'(':
				{
					FunctionType* p = oType.CreateNewFT();
					if(!p || !FunctionTypeFromSignature(*p,++pSig))
					{
						oType.Clear();
						return false;
					}
					t.SetFT(p);
					if(bSetReturnType)
					{
						oType.m_returnType = t;
						bSettingReturnTypeDone = true;
					}
					else oType.m_oArgTypes.push_back(t);
				}
				break;
			default:
				oType.Clear();
				return false;
			}

			if(bSettingReturnTypeDone)
			{
				//++pSig;
				break;
			}
		}

		oType.m_bReturnSpecified = bSettingReturnTypeDone;

		return true;
	}
private:
	void Clear()
	{
		m_returnType.m_p = 0;
		m_returnType.m_t = Int;
		m_bReturnSpecified = false;

		m_oArgTypes.clear();
		/*for(std::vector<FunctionType*>::iterator i=m_subFT.begin();
			i!=m_subFT.end();
			++i)
			delete *i;*/
		m_subFT.clear();
	}

	FunctionType* CreateNewFT()
	{
		FunctionType* p = new FunctionType();
		
		CSimpleObjCont<FunctionType> s;
		m_subFT.push_back(s);
		m_subFT.back().GetObjPtrRef() = p;

		return p;
	}
public:
	std::vector<SmType> m_oArgTypes;
	SmType m_returnType;
	std::list<CSimpleObjCont<FunctionType> > m_subFT;
	bool m_bReturnSpecified;
};

template<class CharType>
std::basic_ostream<CharType>& operator << (std::basic_ostream<CharType>& cs,const FunctionType& o);

template<class CharType>
std::basic_ostream<CharType>& operator << (std::basic_ostream<CharType>& cs,const SmType& o)
{
	switch(o.m_t)
	{
	case Int:
		if((o.m_mod & Ref_Modifier) == Ref_Modifier) cs << L'r';
		cs << L'i';
		break;
	case String:
		if((o.m_mod & Ref_Modifier) == Ref_Modifier) cs << L'r';
		cs << L's';
		break;
	case Bool:
		if((o.m_mod & Ref_Modifier) == Ref_Modifier) cs << L'r';
		cs << L'b';
		break;
	case ObjRefType:
		if(o.m_p) cs << L'(' << *(o.m_p);
		break;
	}

	return cs;
}

template<class CharType>
std::basic_ostream<CharType>& operator << (std::basic_ostream<CharType>& cs,const FunctionType& o)
{
	std::copy(o.m_oArgTypes.begin(),o.m_oArgTypes.end(),std::ostream_iterator<SmType,CharType>(cs));
	if(o.m_bReturnSpecified)
		cs << L')' << o.m_returnType;
	return cs;
}
