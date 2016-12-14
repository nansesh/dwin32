#pragma once

#include <string>
#include <map>
#include <cassert>
#include <sstream>

class RefType
{
public:
	virtual std::wstring GetStringForm() = 0;
	virtual ~RefType()
	{}
};

class ObjectsManager;

class HardReference
{
public:
	HardReference& operator = (const HardReference& o)
	{
		if(this==&o)
			return *this;
		this->Reassign(o);

		return *this;
	}
	const RefType* Value() const
	{
		return m_pReferred;
	}
	RefType* Value()
	{
		return m_pReferred;
	}
private:
	friend class ObjectsManager;
	friend class HardRefHolder;

	HardReference(const HardReference&);
	HardReference(RefType* pReferred,ObjectsManager* obm)
		:m_pReferred(pReferred)
		,m_obm(obm)
	{}

	void Reassign(const HardReference& oFrom);

	RefType* m_pReferred;
	ObjectsManager* m_obm;
};

class HardRefHolder;
class ObjectsManager
{
public:
	~ObjectsManager()
	{
		for(std::map<HardReference*,int>::iterator i = m_hard_refs.begin();
			i!=m_hard_refs.end();
			++i)
		{
			delete i->first;
		}
	}

	void AddRef(RefType* p,HardRefHolder&);

	HardReference* AddRef(RefType* p)
	{
		if(!p)
			return 0;

		int& ref_count = m_ref_count[p];
		if(ref_count<0)
		{
			assert(L"[AddRef] ref_count is <0"==0);
		}

		++ref_count;

		HardReference* hrf = new HardReference(p,this);
		m_hard_refs[hrf] = 0;

		return hrf;
	}

	bool ReleaseOwnerShip(HardReference* hrf)
	{
		return this->Release(hrf,false);
	}

	bool TakeOwnerShip(HardReference* hrf,RefType* pReferred)
	{
		if(!hrf)
			return false;

		if(hrf->m_obm==this)
			return true;

		Ref_type_container::iterator i_referred = m_ref_count.find(pReferred);
		if(i_referred==m_ref_count.end())
			return false;

		if(!hrf->m_obm->ReleaseOwnerShip(hrf))
			return false;

		assert(hrf->m_obm==0);
		
		hrf->m_obm = this;
		m_hard_refs[hrf] = 0;

		++(i_referred->second);
		hrf->m_pReferred = pReferred;

		return true;
	}

	bool Release(HardReference* hrf, bool bDelete)
	{
		if(!hrf)
			return false;

		if(hrf->m_obm!=this)
			return false;
		
		std::map<HardReference*,int>::iterator i = m_hard_refs.find(hrf);
		if(i==m_hard_refs.end())
		{
			assert("HardReference::m_obm of the argument is 'this' ObjectsManager object... but this is not able to find the HardReference object in its own map"==0);
			return false;
		}

		Ref_type_container::iterator i_referred = m_ref_count.find(hrf->m_pReferred);
		if(i_referred==m_ref_count.end())
		{
			assert("The object referred by the argument HardReference (which is owned by this ObjectsManager) is not owned by this ObjectsManager"==0);
			return false;
		}

		Release(i_referred);

		hrf->m_pReferred = 0;
		hrf->m_obm = 0;

		m_hard_refs.erase(i);

		if(bDelete)
			delete hrf;

		return true;
	}

	bool Repoint(HardReference& to_be_changed,const HardReference& ref_target)
	{
		Ref_type_container::iterator i = m_ref_count.find(to_be_changed.m_pReferred);
		Ref_type_container::iterator i_right = m_ref_count.find(ref_target.m_pReferred);

		if(i==m_ref_count.end() || i_right==m_ref_count.end())
		{
			return false;
		}

		Release(i);
		// not removing the item from the map when the reference count becomes 0.

		++(i_right->second);

		to_be_changed.m_pReferred = ref_target.m_pReferred;
		return true;
	}

private:
	typedef std::map<RefType*,int> Ref_type_container;
	void Release(Ref_type_container::iterator& i)
	{
		if((--(i->second))<=0)
		{
			delete i->first;
		}
	}
	
	std::map<RefType*,int> m_ref_count;
	std::map<HardReference*,int> m_hard_refs;
};

inline void HardReference::Reassign(const HardReference& oFrom)
{
	if(this->m_obm!=oFrom.m_obm)
	{
		oFrom.m_obm->TakeOwnerShip(this,oFrom.m_pReferred);
		return;
	}

	this->m_obm->Repoint(*this,oFrom);
}

class HardRefHolder
{
public:
	HardRefHolder():m_p(0)
	{}

	HardRefHolder(const HardRefHolder& o):m_p(0)
	{
		if(o.m_p)
		{
			m_p = o.m_p->m_obm->AddRef(o.m_p->Value());
		}
	}

	~HardRefHolder()
	{
		this->Clear();
	}

	void Clear()
	{
		if(m_p)
		{
			m_p->m_obm->Release(m_p,true);
			m_p = 0;
		}
	}

	HardRefHolder& operator = (const HardRefHolder& o)
	{
		if(!m_p)
		{
			if(!o.m_p) return *this;
			m_p = o.m_p->m_obm->AddRef(o.m_p->Value());
			return *this;
		}
		
		if(!o.m_p)
		{
			this->Clear();
			return *this;
		}

		*m_p = *o.m_p;
		return *this;
	}

	HardReference* operator -> ()
	{
		return m_p;
	}

	const HardReference* operator -> () const
	{
		return m_p;
	}

	bool operator !() const
	{
		return m_p==0;
	}
	
	friend class ObjectsManager;

private:
	HardReference* m_p;
};

inline void ObjectsManager::AddRef(RefType* p,HardRefHolder& hrf)
{
	HardReference* pHrf = this->AddRef(p);
	if(!pHrf)
		return;

	hrf.Clear();
	hrf.m_p = pHrf;
}

enum Types
{
	Int = 1,
	Bool = 2,
	String = 3,
	ObjRefType = 4
};

enum TypeModifiers
{
	Modifier_None = 0,
	Ref_Modifier = 1
};

inline std::wstring SimpleCharTypeExpand(const char* p)
{
	std::wstring str;
	while(*p)
		str.push_back((wchar_t)(*(p++)));
	return str;
}

inline std::string SimpleCharTypeExpand(const wchar_t* p)
{
	std::string str;
	while(*p)
		str.push_back((char)(*(p++)));
	return str;
}

struct SOperand
{
	SOperand()
		:m_nValue(0)
		,m_bValue(false)
		,m_type(String)
		,m_bAddressFlag(false)
	{
	}

	SOperand(bool b)
		:m_nValue(0)
		,m_bValue(b)
		,m_type(Bool)
		,m_bAddressFlag(false)
	{
	}

	SOperand(const std::wstring& strVal)
		:m_strValue(strVal)
		,m_nValue(0)
		,m_bValue(false)
		,m_type(String)
		,m_bAddressFlag(false)
	{
		
	}

	SOperand(const HardRefHolder& hrf)
		:m_nValue(0)
		,m_bValue(false)
		,m_refType(hrf)
		,m_type(ObjRefType)
		,m_bAddressFlag(false)
	{
	}

	template<class CharT>
	void SetValue(const std::basic_string<CharT>&);

	template<>
	void SetValue<wchar_t>(const std::wstring& val)
	{
		ResetAllValues();
		
		m_type = String;
		m_strValue = val;
	}

	void SetValue(const wchar_t* pVal)
	{
		ResetAllValues();

		m_type = String;
		m_strValue = pVal!=0?pVal:L"";
	}

	void SetValue(int nI)
	{
		ResetAllValues();

		m_type = Int;
		m_nValue = nI;
	}
	
	void SetValue(bool b)
	{
		ResetAllValues();

		m_type = Bool;
		m_bValue = b;
	}

	void SetValue(const HardRefHolder& hrf)
	{
		ResetAllValues();

		m_type = ObjRefType;
		m_refType = hrf;
	}
	
	std::wstring& GetStringValRef()
	{
		ResetAllValues();
		m_type = String;

		return m_strValue;
	}

	Types GetType() const
	{
		return m_type;
	}

	const std::wstring& GetStringValue() const
	{
		return m_strValue;
	}

	int GetIntValue() const
	{
		return m_nValue;
	}

	bool GetBoolValue() const
	{
		return m_bValue;
	}

	HardRefHolder& GetObjRef()
	{
		return m_refType;
	}

	bool TypeEquals(const SOperand& op) const
	{
		return this->m_type==op.m_type;
	}
	
	// If the type is bool, it returns the value of the boolean object it
	// contains. Else if the type is integer returns true if the value is not
	// zero.
	//
	bool ToBool() const
	{
		if(m_type==Bool)
			return m_bValue;
		
		if(m_type==Int)
			return m_nValue!=0;

		if(m_type==ObjRefType)
			return !!m_refType;

		return m_strValue.size()>0;
	}

	int ToInt() const
	{
		if(m_type==Bool)
			return m_bValue;
		
		if(m_type==Int)
			return m_nValue;

		if(m_type==ObjRefType)
			return !!m_refType;

		if(m_strValue.size()<=0)
			return 0;

		int n = 0;
		const wchar_t* p = m_strValue.c_str() + (m_strValue.size()>2?((m_strValue[0]==L'0' && m_strValue[1]==L'x')?2:0):0);
		std::wistringstream wis(p);
		if(p==m_strValue.c_str())
		{
			wis >> n;
		}
		else
		{
			wis >> std::hex >> n;
		}

		return n;
	}

	bool Negate()
	{
		if(m_type==Bool)
		{
			m_bValue=!(m_bValue);
			return true;
		}

		if(m_type==Int)
		{
			m_nValue = !(m_nValue);
			return true;
		}

		return false;
	}

	std::wstring ToString() const
	{
		if(m_type==String)
			return m_strValue;
		std::wostringstream wos;
		
		if(m_type==Int)
		{
			wos << m_nValue;
		}
		else if(m_type==Bool)
		{
			wos << (m_bValue?L"true":L"false");
		}
		else if(m_type==ObjRefType)
		{
			wos << L"ObjRefType " << (!!m_refType?SimpleCharTypeExpand(typeid(*(m_refType->Value())).name()).c_str():L"null");
		}

		return wos.str();
	}

	template<class Type>
	const Type& GetValueBasedOnType() const
	{
		Type t;
		return t;
	}

	template<>
	const std::wstring& GetValueBasedOnType<std::wstring>() const
	{
		return m_strValue;
	}

	template<>
	const int& GetValueBasedOnType<int>() const
	{
		return m_nValue;
	}

	template<>
	const bool& GetValueBasedOnType<bool>() const
	{
		return m_bValue;
	}

	template<>
	const HardRefHolder& GetValueBasedOnType<HardRefHolder>() const
	{
		return m_refType;
	}

	void SetReferenceFlag(bool b)
	{
		m_bAddressFlag = b;
	}

	bool IsReference() const
	{
		return m_bAddressFlag;
	}

private:
	void ResetAllValues()
	{
		m_strValue = L"";
		m_nValue = 0;
		m_bValue = false;
		m_refType.Clear();

		m_bAddressFlag = false;
	}

	std::wstring m_strValue;
	int m_nValue;
	bool m_bValue;
	HardRefHolder m_refType;

	Types m_type;

	// Attribute about the data - if true the value of the string or integer is a reference
	// and not a value itself.
	bool m_bAddressFlag;
};

inline void AddSOperand(const SOperand& l,const SOperand& r,SOperand& result)
{
	if(l.GetType()==String || r.GetType()==String)
	{
		std::wstring str_result = l.ToString();
		str_result += r.ToString();

		result.SetValue(str_result);
	}
	else
	{
		int n_ret = l.ToInt();
		n_ret += r.ToInt();
		result.SetValue(n_ret);
	}
}
