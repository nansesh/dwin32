#pragma once

#include "SOperand.h"
#include "FunSigSyntax.h"
#include "EtorInterfaces.h"
#include <list>

#include <windows.h>
#include <memory.h>
#include <iostream>

///**
//* @brief	windows call-able wrapper.
//*
//*/
class Wcw : public RefType
{
public:
	Wcw(const FunctionType& ft,HardRefHolder hrh,IContextProvider& cp)
		:m_pEP(0)
		,m_hrh(hrh)
		,m_cp(cp)
		,m_curFunArg(m_oFuncArgList.begin())
		,m_pAF(0)
		,m_myReturnType(ft.m_returnType.m_t)
	{
		m_pEP = MakeFunction(ft);
		if(!m_hrh->Value())
			return;

		m_pAF = dynamic_cast<AbstractFunction*>(m_hrh->Value());
	}

	~Wcw();

	void* GetEntryPoint()
	{
		return m_pEP;
	}

	virtual std::wstring GetStringForm()
	{
		return L"Wcw(...){}";
	}

private:
	Wcw(const Wcw&);
	Wcw& operator = (const Wcw&);

	void* MakeFunction(const FunctionType& o);

	static void (__stdcall Wcw::*GetFunction(Types t))(DWORD d)
	{
		switch(t)
		{
		case Int:
			return &StoreIntArgument;
		case String:
			return &StoreStringArgument;
		case ObjRefType:
			return &StoreFunctionArgument;
		default:
			return &TestFunction;
		}
	}

	void __stdcall TestFunction/*StoreStringArgument*/(DWORD d)
	{
		//std::wcout << this->m_str.c_str() << L" --- value from thunk = " << d << std::endl;
	}

	void __stdcall StoreIntArgument(DWORD d)
	{
		SOperand s;
		s.SetValue((int)d);
		m_oArgs.insert(m_oArgs.begin(),s);
	}

	void __stdcall StoreFunctionArgument(DWORD d)
	{
	}

	void __stdcall StoreStringArgument(DWORD d)
	{
		SOperand s;
		s.SetValue(reinterpret_cast<const wchar_t*>(d));
		m_oArgs.insert(m_oArgs.begin(),s);
	}

	DWORD __stdcall ExecuteFunction()
	{
		//std::wcout << this->m_str.c_str() << L" --- executing function" << std::endl;
		if(!m_pAF)
			return 0;

		
		m_cp.GetContext().EnterACallContext(m_pAF->GetArgNames(),m_oArgs,true);
		SOperand r = m_pAF->Execute(m_cp);
		m_cp.GetContext().LeaveCallContext();

		if(m_myReturnType!=Int)
		{
			std::wcout << L"setting return value of 0" << std::endl;
			r.SetValue(0);
		}
		else
		{
			r.SetValue(r.ToInt());
		}

		std::wcout << L"going to return int value of - " << (DWORD)r.GetValueBasedOnType<int>() 
			<< std::endl;

		m_oArgs.clear();
		m_curFunArg = m_oFuncArgList.begin();

		return (DWORD)r.GetValueBasedOnType<int>();
	}

	void AddFunSigToArgList(const FunctionType& o)
	{
		m_oFuncArgList.push_back(o);
		m_curFunArg = m_oFuncArgList.begin();
	}

	std::list<FunctionType> m_oFuncArgList;
	std::vector<SOperand> m_oArgs;
	void* m_pEP;
	HardRefHolder m_hrh;
	IContextProvider& m_cp;
	std::list<FunctionType>::iterator m_curFunArg;
	AbstractFunction* m_pAF;
	Types m_myReturnType;
};
