// SmallExprEtor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost\spirit.hpp>
#include <boost\spirit\tree\ast.hpp>
#include <boost\spirit\tree\tree_to_xml.hpp>

#include "SOperand.h"
#include "EtorInterfaces.h"
#include "FunSigSyntax.h"
#include "Wcw.h"

#include <string>
#include <iostream>
#include <typeinfo>
#include <iterator>
#include <stdexcept>
#include <map>
#include <stack>
#include <windows.h>
#include <comutil.h>
#include <vector>

namespace
{
	//_com_error e(0);

	template<class CharT>
	int t_isspace(CharT);

	template<>
	int t_isspace<char>(char a)
	{
		return ::isspace(a);
	}

	template<>
	int t_isspace<wchar_t>(wchar_t a)
	{
		return ::iswspace(a);
	}

	// adapter class to adapt pointer based zero terminated array access to scanner.
	template<class T>
	class ZeroTerminateArray_to_Scanner
	{
	public:
		typedef T value_t;

		explicit ZeroTerminateArray_to_Scanner(T* p)
			:m_p(p)
		{
		}

		// All of the following methods are needed by scanner users.
		bool at_end() const
		{
			return (*m_p)==0;
		}

		const ZeroTerminateArray_to_Scanner& operator ++ () const
		{
			++m_p;
			return *this;
		}

		T operator * () const
		{
			return *m_p;
		}

		mutable T* m_p;
	};

	// adapter class to adapt a pointer based array access to scanner.
	template<class IteratorType,class ValueType>
	class Pointer_to_Scanner
	{
	public:
		typedef ValueType value_t;

		Pointer_to_Scanner(IteratorType p,IteratorType pEnd)
			:m_p(p)
			,m_end(pEnd)
		{
		}

		bool at_end() const
		{
			return m_p==m_end;
		}

		const Pointer_to_Scanner& operator ++ () const
		{
			++m_p;
			return *this;
		}

		ValueType operator * () const
		{
			return *m_p;
		}

		mutable IteratorType m_p;
		IteratorType m_end;
	};

	template<class CharT>
	struct string_parser
	{
		typedef std::basic_string<CharT> result_t;

		template <typename ScannerT>
		int operator()(ScannerT const& scan, result_t& result) const
		{
			if (scan.at_end())
				return -1;

			result.clear();
			result.reserve(128);

			int len = 0;

			typename ScannerT::value_t ch = *scan;
			if(ch!=L'\'')
				return -1;
			
			++len;
			++scan;

			bool bFoundEndOfString = false;
			while(!scan.at_end())
			{
				ch = *scan;

				if(ch==L'\\')
				{
					++len;
					++scan;
					if(!scan.at_end())
					{
						typename ScannerT::value_t escapedChar = *scan;
						if(escapedChar==L'r')
						{
							result += L'\r';
						}
						else if(escapedChar==L'n')
						{
							result += L'\n';
						}
						else if(escapedChar==L'x')
						{
							std::wstring temp;
							int count = 0;
							for(;count<4;++count)
							{
								++len;++scan;
								if(scan.at_end())
									break;
								temp += *scan;
							}

							if(count!=4)
								break;

							std::wistringstream wis(temp);
							int x = 0;
							wis >> std::hex >> x;
							if(!wis.eof())
							{
								result.append(temp);
							}
							else
							{
								result.append(1,(wchar_t)x);
							}
						}
						else
						{
							result += escapedChar;
						}
					}
					else
					{
						break;
					}
				}
				else if(ch==L'\'')
				{
					++len;
					++scan;

					bFoundEndOfString = true;
					break;
				}
				else
				{
					result += *scan;
				}

				++len;
				++scan;
			}

			return bFoundEndOfString?len:-1;
		}
	};

	boost::spirit::functor_parser<string_parser<wchar_t> > string_parser_p;
	
	struct SmExpressionGrammer : public boost::spirit::grammar<SmExpressionGrammer>
	{
		template<class ScannerT>
		struct definition
		{
			definition(const SmExpressionGrammer& oGrammer)
			{
				/*Expression = ExpFactor >> *(
					( boost::spirit::root_node_d[boost::spirit::ch_p(L'=')] >> ExpFactor)
					);*/

				Expression =  
					( ExpFactor >> boost::spirit::root_node_d[boost::spirit::ch_p(L'=')] >> Expression)
					| ExpFactor
					;

				ExpFactor = ExpUnit >> *((boost::spirit::root_node_d[boost::spirit::ch_p(L'+')] >> ExpUnit) 
						| ( boost::spirit::root_node_d[boost::spirit::ch_p(L'?')] >> TernaryUnit));

				//BinaryOpPair = BinaryOp >> ExpUnit;

				ExpUnit = (Variable | Constant | ParenExp) >> *(boost::spirit::root_node_d[boost::spirit::ch_p(L'(')] >> *(Expression >> *(boost::spirit::ch_p(L',') >> Expression))  >> boost::spirit::ch_p(L')'));
				
				Variable = boost::spirit::leaf_node_d
					[boost::spirit::lexeme_d
						[
							(boost::spirit::range_p(L'a',L'z') | boost::spirit::range_p(L'A',L'Z') | boost::spirit::ch_p(L'_'))
							>> *(boost::spirit::range_p(L'a',L'z') 
								| boost::spirit::range_p(L'A',L'Z') 
								| boost::spirit::ch_p(L'_') 
								| boost::spirit::range_p(L'0',L'9')
								)
						]
					];
				
				//VarName = boost::spirit::lexeme_d
				//	[
				//		+(boost::spirit::range_p(L'a',L'z') | boost::spirit::range_p(L'A',L'Z') | boost::spirit::ch_p(L'_'))
				//		//>> *(boost::spirit::range_p(L'a',L'z') 
				//		//| boost::spirit::range_p(L'A',L'Z') 
				//		////| boost::spirit::ch_p(L'_') 
				//		//| boost::spirit::range_p(L'0',L'9')
				//		//)
				//	];

				Constant = StringConstant;
				StringConstant = boost::spirit::leaf_node_d[string_parser_p];
				
				ParenExp = boost::spirit::inner_node_d[boost::spirit::ch_p(L'(') >> Expression >> boost::spirit::ch_p(L')')];
				TernaryUnit = Expression >> boost::spirit::ch_p(L':') >> Expression;
				
				//BinaryOp = boost::spirit::ch_p(L'+');
			}

			boost::spirit::rule<ScannerT> Expression,/*BinaryOpPair*/ ExpUnit /*BinaryOp*/
				,/*TernaryExp*/ Variable,Constant,ParenExp,TernaryUnit
				,ExpFactor
				//,VarName
				,StringConstant;

			const boost::spirit::rule<ScannerT>& start() const
			{ 
				return Expression;
			}
		};

		template<class CharT>
		static void GetPlusSymbol(CharT& ch);

		template<>
		static void GetPlusSymbol<char>(char& ch)
		{
			ch = '+';
		}
		template<>
		static void GetPlusSymbol<wchar_t>(wchar_t& ch)
		{
			ch = L'+';
		}

		template<class CharT>
		static void GetQuestionMark(CharT& ch);
		template<>
		static void GetQuestionMark<char>(char& ch)
		{ch='?';}
		template<>
		static void GetQuestionMark<wchar_t>(wchar_t& ch)
		{ch=L'?';}

		template<class CharT>
		static void GetEqualSymbol(CharT& ch);
		template<>
		static void GetEqualSymbol<char>(char& ch)
		{ch='=';}
		template<>
		static void GetEqualSymbol<wchar_t>(wchar_t& ch)
		{ch=L'=';}

		template<class CharT>
		static void GetOpenParenSymbol(CharT& ch);
		template<>
		static void GetOpenParenSymbol<char>(char& ch)
		{ch='(';}
		template<>
		static void GetOpenParenSymbol<wchar_t>(wchar_t& ch)
		{ch=L'(';}
	};

	template<class T>
	void ReadLine(std::basic_istream<T,std::char_traits<T> >& is,std::basic_string<T>& str)
	{
		T oA[256] = {0};
		str.reserve(256);
		is.getline(&oA[0],sizeof(oA)/sizeof(oA[0]) - 1);

		str = &oA[0];
	}

	class VariableNameAddress : public Address
	{
	public:
		explicit VariableNameAddress(const std::wstring& s)
			:m_address(s)
		{}

		virtual std::wstring ToString() const
		{
			return m_address;
		}

		std::wstring m_address;
	};

	class StringMem : public Memory
	{
	public:
		explicit StringMem(std::map<std::wstring,SOperand>& mp)
			:m_mp(mp)
		{}

		virtual void Read(const Address& a,SOperand& op)
		{
			op = m_mp[a.ToString()];
		}
		
		virtual void Write(const Address& a, const SOperand& op)
		{
			m_mp[a.ToString()] = op;
		}

		std::map<std::wstring,SOperand>& m_mp;
	};

	class CallContextMemory : public Memory
	{
	public:
		CallContextMemory(const std::vector<std::wstring>& vec_args
			,std::vector<SOperand>& vec_values
			)
			:m_vec_args(vec_args)
			,m_vec_values(vec_values)
		{
			int size_diff = m_vec_args.size() - m_vec_values.size();
			for(int i=0;i<size_diff;++i)
				m_vec_values.push_back(SOperand());
		}

		virtual void Read(const Address& a,SOperand& op)
		{
			std::vector<std::wstring>::const_iterator i = 
				std::find(m_vec_args.begin(),m_vec_args.end(),a.ToString());
			if(i==m_vec_args.end())
				throw MemoryException("var not arg");

			int index = i - m_vec_args.begin();
			op = m_vec_values[index];
		}
		
		virtual void Write(const Address& a, const SOperand& op)
		{
			std::vector<std::wstring>::const_iterator i = 
				std::find(m_vec_args.begin(),m_vec_args.end(),a.ToString());
			if(i==m_vec_args.end())
				throw MemoryException("var not arg");

			int index = i - m_vec_args.begin();
			
			m_vec_values[index] = op;
		}

		const std::vector<std::wstring>& m_vec_args;
		std::vector<SOperand>& m_vec_values;
	};

	class TransparentCallContext : public CallContextMemory
	{
	public:
		TransparentCallContext(const std::vector<std::wstring>& vec_args
			,std::vector<SOperand>& vec_values
			,Memory& parent
			):CallContextMemory(vec_args,vec_values)
			,m_parent(parent)
		{}

		virtual void Read(const Address& a,SOperand& op)
		{
			m_parent.Read(a,op);
		}

		virtual void Write(const Address& a, const SOperand& op)
		{
			m_parent.Write(a,op);
		}

		Memory& m_parent;
	};

	class LocalityBasedMemoryStack : public Memory
	{
	public:
		explicit LocalityBasedMemoryStack(Memory* pMain)
			:m_pMain(pMain)
		{
			if(!m_pMain)
				throw MemoryException("LocalityBasedMemoryStack cannot be constructed. Null argument for pMain is not valid");
		}

		virtual void Read(const Address& a,SOperand& op)
		{
			if(m_mem_stack.empty())
				return m_pMain->Read(a,op);

			try
			{
				return m_mem_stack.top()->Read(a,op);
			}
			catch(MemoryException&)
			{
				return m_pMain->Read(a,op);
			}
		}
		
		virtual void Write(const Address& a, const SOperand& op)
		{
			if(m_mem_stack.empty())
				return m_pMain->Write(a,op);

			try
			{
				return m_mem_stack.top()->Write(a,op);
			}
			catch(MemoryException&)
			{
				return m_pMain->Write(a,op);
			}
		}

		Memory* CurrentContextMem()
		{
			if(m_mem_stack.empty())
				return 0;

			return m_mem_stack.top();
		}

		Memory* CurrentOrMain()
		{
			if(m_mem_stack.empty())
				return m_pMain;

			return m_mem_stack.top();
		}

		void Push(Memory* pmem)
		{
			m_mem_stack.push(pmem);
		}

		Memory* Pop()
		{
			Memory* p = m_mem_stack.top();
			m_mem_stack.pop();
			return p;
		}

	private:
		std::stack<Memory*> m_mem_stack;
		Memory* m_pMain;
	};

	class SimpleComputationContext : public Context
	{
	public:
		SimpleComputationContext(ObjectsManager& obm)
			:m_mp()
			,m_memory(m_mp)
			,m_local_mem(&m_memory)
			,m_obm(obm)
		{}

		std::map<std::wstring,SOperand>& GetMemMap()
		{
			return m_mp;
		}

		virtual Memory& GetMemory()
		{
			return m_local_mem;
		}

		virtual void EnterACallContext(const std::vector<std::wstring>& arg_names,std::vector<SOperand>& arg_values,bool bNewNameContext = true)
		{
			CallContextMemory* pMem = ( bNewNameContext?new CallContextMemory(arg_names,arg_values) 
				: new TransparentCallContext(arg_names,arg_values,*(m_local_mem.CurrentOrMain())) 
				);
			m_local_mem.Push(pMem);
		}

		virtual void LeaveCallContext()
		{
			Memory* pMem = m_local_mem.Pop();
			delete pMem;
		}

		virtual std::auto_ptr<ArgsArray> GetArguments()
		{
			std::auto_ptr<ArgsArray> oAr;

			CallContextMemory* pMem = dynamic_cast<CallContextMemory*>(m_local_mem.CurrentContextMem());
			if(!pMem)
				return oAr;

			oAr.reset(new ArgsArray(pMem->m_vec_values));
			return oAr;
		}

		virtual ObjectsManager* GetObjMgr()
		{
			return &m_obm;
		}

		std::map<std::wstring,SOperand> m_mp;
		StringMem m_memory;
		LocalityBasedMemoryStack m_local_mem;
		ObjectsManager& m_obm;
	};

	template<class CharT,class SomeType>
	class Evaluator;

	template<class CharT,class SomeType>
	class EtorBasicEtable
	{
	public:
		typedef boost::spirit::tree_node<boost::spirit::node_val_data<const CharT*,SomeType> > node_type;
		typedef Evaluator<CharT,SomeType> EtorType;
		virtual bool Evaluate(node_type&,EtorType&,SOperand&) = 0;

		typename node_type::parse_node_t::iterator_t GetFirstNonWhiteSpaceIndex(node_type& nd)
		{
			for(typename node_type::parse_node_t::iterator_t i = nd.value.begin();
				i!=nd.value.end();
				++i)
			{
				if(!t_isspace(*i))
				{
					return i;
				}
			}

			return nd.value.end();
		}
	};

	/*template<class CharT>
	class IEtorBasicEtableFactory
	{
	public:
		virtual std::auto_ptr<EtorBasicEtable<CharT> > GetEvaluator(const CharT*) = 0;
	};*/

	template<class CharT,class SomeType>
	class Evaluator : public IContextProvider
	{
	public:
		explicit Evaluator(Context& context)
			:m_context(context)
		{
		}

		void AddSubEvaluator(EtorBasicEtable<CharT,SomeType>* pEtor)
		{
			if(pEtor)
				m_vecSubEvaluators.push_back(pEtor);
		}
		
		bool Evaluate(typename EtorBasicEtable<CharT,SomeType>::node_type& nd,SOperand& op)
		{
			bool bEvRes = false;

			for(container_t::iterator i = m_vecSubEvaluators.begin();
				i!=m_vecSubEvaluators.end();
				++i)
			{
				bEvRes = (*i)->Evaluate(nd,*this,op);
				if(bEvRes)
					break;
			}

			return bEvRes;
		}

		Context& GetContext()
		{
			return m_context;
		}

	private:
		typedef std::vector<EtorBasicEtable<CharT,SomeType>*> container_t;
		container_t m_vecSubEvaluators;
		Context& m_context;
	};

	template<class CharT,class SomeType>
	class EtorStrCnstEtable:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;
		//static std::auto_ptr<EtorBasicEtable<CharT> > MakeEvaluator(const CharT* pStr)
		//{
		//	// string constants would start with '
		//	// if it does not this evaluator would not be able to evaluate the char type.
		//	// so we just return null.
		//	
		//	int a = '\'';
		//	if(!pStr || *pStr!=a)
		//		return std::auto_ptr<EtorBasicEtable<CharT> >(0);

		//	return std::auto_ptr<EtorBasicEtable<CharT> >(new EtorStrCnstEtable<CharT>());
		//}

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType&,SOperand& op)
		{
			// string constants would start with '
			// if it does not this evaluator would not be able to evaluate the node.
			// so we just return null.
			
			if(nd.value.begin()==nd.value.end())
				return false;

			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			const CharT ch1 = *(i);

			int a = '\'';
			if(ch1!=a)
				return false;

			string_parser<CharT> sp;
			std::basic_string<CharT> str;
			Pointer_to_Scanner<typename node_type::parse_node_t::iterator_t,CharT> sc(i,nd.value.end());
			sp(sc,str);
			op.SetValue(str);

			return true;
		}
	};

	template<class CharT,class SomeType>
	class EtorPlusEtable:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType& rootEtor,SOperand& op)
		{
			if(nd.value.begin()==nd.value.end())
				return false;

			CharT ch;
			SmExpressionGrammer::GetPlusSymbol(ch);

			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			if(ch!=(*i))
				return false;

			if(nd.children.size()<2)
				throw EvaluatorException("+ operator does not have the required two operands");

			SOperand left;
			if(!rootEtor.Evaluate(nd.children[0],left))
			{
				return false;
			}
			
			SOperand right;
			if(!rootEtor.Evaluate(nd.children[1],right))
			{
				return false;
			}

			if(left.IsReference())
				rootEtor.GetContext().GetMemory().Read(VariableNameAddress(left.ToString()),left);

			//std::wstring result = left.ToString();

			if(right.IsReference())
				rootEtor.GetContext().GetMemory().Read(VariableNameAddress(right.ToString()),right);

			// AddSOperand will do addition based on the types of left and right.
			AddSOperand(left,right,op);
			
			//result += right.ToString();

			//op.SetValue(result);

			return true;
		}
	};

	template<class CharT,class SomeType>
	class EtorArithmeticIf:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType& rootEtor,SOperand& op)
		{
			CharT ch = 0; 
			SmExpressionGrammer::GetQuestionMark(ch);

			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			if(ch!=(*i))
				return false;

			if(nd.children.size()<2)
				throw EvaluatorException("?: operator does not have the required number operands");

			SOperand condition;
			if(!rootEtor.Evaluate(nd.children[0],condition))
			{
				return false;
			}

			if(nd.children[1].children.size()<3)
				throw EvaluatorException("?: operator does not have the required number operands - second part needs two operands.");

			int nIndex = 0;
			if(condition.IsReference())
			{
				rootEtor.GetContext().GetMemory().Read(VariableNameAddress(condition.ToString()),condition);
			}

			if(!condition.ToBool())
			{
				nIndex = 2;
			}

			return rootEtor.Evaluate(nd.children[1].children[nIndex],op);
		}
	};

	template<class CharT,class SomeType>
	class EtorFunctionCall:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType& rootEtor,SOperand& op)
		{
			CharT ch = 0; 
			SmExpressionGrammer::GetOpenParenSymbol(ch);

			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			if(ch!=(*i))
				return false;

			if(nd.children.size()<1)
				throw EvaluatorException("() - function call operator - does not have the required number operands");

			SOperand func;
			if(!rootEtor.Evaluate(nd.children[0],func))
			{
				return false;
			}

			if(func.IsReference())
			{
				rootEtor.GetContext().GetMemory().Read(VariableNameAddress(func.ToString()),func);
			}

			if(func.GetType()!=ObjRefType || !(func.GetObjRef()))
			{
				throw EvaluatorException("argument does not evaluate to a function object type");
			}

			RefType* pObj = func.GetObjRef()->Value();
			AbstractFunction* paf = dynamic_cast<AbstractFunction*>(pObj);
			if(!paf)
			{
				throw EvaluatorException("argument is not a function");
			}

			bool bHandleRefArgs = paf->HandlesReferenceArguments();

			std::vector<SOperand> vec_args;
			SOperand dummy;

			if(nd.children.size()>2)
			{
				// have to skip "," hence i+=2
				for(int i=1;i<nd.children.size();i+=2)
				{
					vec_args.push_back(dummy);
					if(!rootEtor.Evaluate(nd.children[i],vec_args.back()))
					{
						std::ostringstream os;
						os << "evaluation of one of the argument failed. index = "
							<< i;
						throw EvaluatorException(os.str().c_str());
					}
					else
					{
						if(vec_args.back().IsReference() && !bHandleRefArgs)
						{
							rootEtor.GetContext().GetMemory().Read(VariableNameAddress(vec_args.back().ToString()),vec_args.back());
						}
					}
				}
			}

			bool bNeedNew = paf->NeedsANewMemoryContext();

			RIIA_GEN_HOLDER(
			{rootEtor.GetContext().EnterACallContext(paf->GetArgNames(),vec_args,bNeedNew);}
				,{ rootEtor.GetContext().LeaveCallContext(); }
				,rootEtor,paf,vec_args,bNeedNew
				);

			//rootEtor.GetContext().EnterACallContext(paf->GetArgNames(),vec_args);
			
			op = paf->Execute(rootEtor);

			//rootEtor.GetContext().LeaveCallContext();

			return true;
		}
	};

	template<class CharT,class SomeType>
	class EtorVariable:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType& rootEtor,SOperand& op)
		{
			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			std::wstring w(i._Checked_iterator_base(),nd.value.end() - i);
			op.SetValue(w);
			op.SetReferenceFlag(true);

			return true;
		}

	};

	template<class CharT,class SomeType>
	class EtorAssignmentOp:public EtorBasicEtable<CharT,SomeType>
	{
	public:
		typedef EtorBasicEtable<CharT,SomeType> base_type;

		virtual bool Evaluate(typename base_type::node_type& nd,EtorType& rootEtor,SOperand& op)
		{
			typename node_type::parse_node_t::iterator_t i = GetFirstNonWhiteSpaceIndex(nd);
			if(i==nd.value.end())
				return false;

			CharT ch = 0;
			SmExpressionGrammer::GetEqualSymbol(ch);

			if(ch!=(*i))
				return false;

			if(nd.children.size()<2)
				throw EvaluatorException("= operator does not have the required two operands");

			SOperand right;
			if(!rootEtor.Evaluate(nd.children[1],right))
			{
				return false;
			}

			if(right.IsReference())
			{
				rootEtor.GetContext().GetMemory().Read(VariableNameAddress(right.ToString()),right);
			}

			SOperand left;
			if(!rootEtor.Evaluate(nd.children[0],left))
			{
				return false;
			}

			if(!left.IsReference())
			{
				throw EvaluatorException("= operator does not have a l-value on the left");
			}

			rootEtor.GetContext().GetMemory().Write(VariableNameAddress(left.ToString()),right);

			op = left;

			return true;
		}
	};

	SOperand Evaluate(Context& compContext,boost::spirit::tree_parse_info<const wchar_t*>& info)
	{
		try
		{
			EtorStrCnstEtable<wchar_t,boost::spirit::nil_t> strCnstEtor;
			EtorPlusEtable<wchar_t,boost::spirit::nil_t> plusEtor;
			EtorArithmeticIf<wchar_t,boost::spirit::nil_t> arthIfEtor;
			EtorAssignmentOp<wchar_t,boost::spirit::nil_t> assignEtor;
			EtorVariable<wchar_t,boost::spirit::nil_t> varEtor;
			EtorFunctionCall<wchar_t,boost::spirit::nil_t> funcCallEtor;

			Evaluator<wchar_t,boost::spirit::nil_t> oEv(compContext);
			oEv.AddSubEvaluator(&strCnstEtor);
			oEv.AddSubEvaluator(&funcCallEtor);
			oEv.AddSubEvaluator(&plusEtor);
			oEv.AddSubEvaluator(&arthIfEtor);
			oEv.AddSubEvaluator(&assignEtor);
			oEv.AddSubEvaluator(&varEtor);

			SOperand result;
			bool bRet = oEv.Evaluate(info.trees[0],result);

			if(bRet)
				std::wcout << L"evaluation result = " << result.ToString().c_str() << std::endl;
			else
				std::wcout << L"evaluation of expression failed" << std::endl;

			return result;
		}
		catch(EvaluatorException& e)
		{
			std::cout << "exception occured during evaluation. Message = " << e.what() << std::endl;
		}

		return SOperand();
	}
}

class TestCasesRunner
{
public:
	TestCasesRunner()
		:compContext(obm)
	{}
	void RunTestCases()
	{
		TestCase1();
		TestCase2();
		TestCase3();
		TestCase4();
		TestCase5();
		TestCase6();
		TestCase7();
		TestCase8();
		VariableNameTestCase();
		FunctionCallTestCase();
	}

	void TestCase1()
	{
		const wchar_t* pStr1 = L"a='123'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"a" && result.IsReference())
		{
			compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
			if(result.ToString()==L"123")
			{
				std::wcout << L"TestCase1 \"" << pStr1 << L"\" succeeded" << std::endl;
			}
			else
			{
				std::wcout << L"TestCase1 \"" << pStr1 << L"\" failed at point 2" << std::endl;
			}
		}
		else
		{
			std::wcout << L"TestCase1 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase2()
	{
		const wchar_t* pStr1 = L"'pqr' + '12\\'3'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"pqr12'3")
		{
			std::wcout << L"TestCase2 \"" << pStr1 << L"\" succeeded" << std::endl;
		}
		else
		{
			std::wcout << L"TestCase2 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase3()
	{
		const wchar_t* pStr1 = L"'lo'?'1   ' : ' 3 3'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"1   ")
		{
			std::wcout << L"TestCase3 \"" << pStr1 << L"\" succeeded" << std::endl;
		}
		else
		{
			std::wcout << L"TestCase3 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase4()
	{
		const wchar_t* pStr1 = L"''?'1   ' : ' 3 3 '";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L" 3 3 ")
		{
			std::wcout << L"TestCase4 \"" << pStr1 << L"\" succeeded" << std::endl;
		}
		else
		{
			std::wcout << L"TestCase4 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase5()
	{
		const wchar_t* pStr1 = L"'1'?'2'?'e':'f':'h'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"e" && !result.IsReference())
		{
			std::wcout << L"TestCase5 \"" << pStr1 << L"\" succeeded" << std::endl;
		}
		else
		{
			std::wcout << L"TestCase5 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase6()
	{
		const wchar_t* pStr1 = L"a='123'?'q':'w'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"a" && result.IsReference())
		{
			compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
			if(result.ToString()==L"q")
			{
				std::wcout << L"TestCase6 \"" << pStr1 << L"\" succeeded" << std::endl;
			}
			else
			{
				std::wcout << L"TestCase6 \"" << pStr1 << L"\" failed at point 2" << std::endl;
			}
		}
		else
		{
			std::wcout << L"TestCase6 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase7()
	{
		const wchar_t* pStr1 = L"a='123' + ''?'q':'w'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"a" && result.IsReference())
		{
			compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
			if(result.ToString()==L"q")
			{
				std::wcout << L"TestCase7 \"" << pStr1 << L"\" succeeded" << std::endl;
			}
			else
			{
				std::wcout << L"TestCase7 \"" << pStr1 << L"\" failed at point 2" << std::endl;
			}
		}
		else
		{
			std::wcout << L"TestCase7 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void TestCase8()
	{
		const wchar_t* pStr1 = L"a=('123' + '')?'lmn':'w'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"a" && result.IsReference())
		{
			compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
			if(result.ToString()==L"lmn")
			{
				std::wcout << L"TestCase8 \"" << pStr1 << L"\" succeeded" << std::endl;
			}
			else
			{
				std::wcout << L"TestCase8 \"" << pStr1 << L"\" failed at point 2" << std::endl;
			}
		}
		else
		{
			std::wcout << L"TestCase8 \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void VariableNameTestCase()
	{
		const wchar_t* pStr1 = L"_qw_1_WE_2='a'";
		SOperand result = EvalExpression(pStr1);
		if(result.ToString()==L"_qw_1_WE_2" && result.IsReference())
		{
			compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
			if(result.ToString()==L"a")
			{
				std::wcout << L"VariableNameTestCase \"" << pStr1 << L"\" succeeded" << std::endl;
			}
			else
			{
				std::wcout << L"VariableNameTestCase \"" << pStr1 << L"\" failed at point 2" << std::endl;
			}
		}
		else
		{
			std::wcout << L"VariableNameTestCase \"" << pStr1 << L"\" failed at point 1" << std::endl;
		}
	}

	void FunctionCallTestCase()
	{
		const wchar_t* pStr1 = L"f(a)(p)";
		SOperand result = EvalExpression(pStr1);
		std::wcout << L"finished function call test case " << std::endl;
	}

private:
	SOperand EvalExpression(const wchar_t* pStr)
	{
		SOperand result;

		if(!pStr)
			return result;

		boost::spirit::tree_parse_info<const wchar_t*> info = boost::spirit::ast_parse(pStr,oGr,boost::spirit::space_p);
		if(info.full)
		{
			if(info.trees.size()>0)
			{
				return Evaluate(compContext,info);
			}
		}
		else
		{
			std::wcout << L"stopped at point " << info.stop << std::endl;
			std::wcout << L"parsing of expression \"" << pStr << L"\" failed" << std::endl;
		}

		return result;
	}
private:
	SmExpressionGrammer oGr;
	ObjectsManager obm;
	SimpleComputationContext compContext;
};

class A
{public: int n;};

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define WIDE_FUNCTION_NAME WIDEN(__FUNCTION__)

class B
{
	public: int n; char a; 
	void func()
	{
		std::wcout << WIDEN(__FUNCTION__)
			<< std::endl;
	}
};

int _tmain1(int argc, _TCHAR* argv[])
{
	TestCasesRunner otcr;
	otcr.RunTestCases();

	return 0;
}

class StringLenFunction : public AbstractFunction
{
public:
	StringLenFunction()
	{
		m_arg_names.push_back(L"str");
	}

	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		cp.GetContext().GetMemory().Read(VariableNameAddress(L"str"),op);
		op.SetValue((int)(op.GetType()==String?op.GetStringValue().size():op.ToString().size()));
		return op;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}

	virtual std::wstring GetStringForm()
	{
		return L"strlen(str){}";
	}

private:
	std::vector<std::wstring> m_arg_names;
};

class PrintFunction : public AbstractFunction
{
public:
	PrintFunction()
	{
		m_arg_names.push_back(L"str");
	}

	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		cp.GetContext().GetMemory().Read(VariableNameAddress(L"str"),op);
		std::wcout << (op.GetType()==String?op.GetStringValue():op.ToString()) << std::endl;
		op.SetValue(1);
		return op;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}

	virtual std::wstring GetStringForm()
	{
		return L"strlen(str){}";
	}

private:
	std::vector<std::wstring> m_arg_names;
};

class LoopFunction : public AbstractFunction
{
public:
	LoopFunction()
	{
		m_arg_names.push_back(L"str");
	}

	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		cp.GetContext().GetMemory().Read(VariableNameAddress(L"str"),op);
		SmExpressionGrammer oGr;
		boost::spirit::tree_parse_info<const wchar_t*> info;
		info = boost::spirit::ast_parse((op.GetType()==String?op.GetStringValue():op.ToString()).c_str(),oGr,boost::spirit::space_p);
		if(!info.full)
		{
			return op;
		}
		
		//std::wcout.setstate(std::ios::failbit);
		
		while(true)
		{
			op = Evaluate(cp.GetContext(),info);
			if(op.IsReference())
			{
				cp.GetContext().GetMemory().Read(VariableNameAddress(op.ToString()),op);
			}

			if(!op.ToBool())
				break;
		}

		//std::wcout.clear();

		return op;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}

	virtual std::wstring GetStringForm()
	{
		return L"loop(str_expression){}";
	}

private:
	std::vector<std::wstring> m_arg_names;
};


class DynamicFunction : public AbstractFunction
{
public:
	DynamicFunction(const std::vector<std::wstring>& vec_arg_names
		,const std::wstring& str_func_body
		)
		:m_arg_names(vec_arg_names)
		,m_func_body(str_func_body)
	{
		SmExpressionGrammer oGr;
		m_info = boost::spirit::ast_parse(m_func_body.c_str(),oGr,boost::spirit::space_p);
		if(!m_info.full)
			throw (int)0;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}

	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand result = Evaluate(cp.GetContext(),m_info);
		if(result.IsReference())
		{
			cp.GetContext().GetMemory().Read(VariableNameAddress(result.ToString()),result);
		}

		return result;
	}

	virtual std::wstring GetStringForm()
	{
		return m_func_body;
	}

private:
	std::vector<std::wstring> m_arg_names;
	std::wstring m_func_body;
	boost::spirit::tree_parse_info<const wchar_t*> m_info;
};

class FunctionMaker : public AbstractFunction
{
public:
	FunctionMaker()
	{
	}

	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		if(!cp.GetContext().GetObjMgr())
			return op;

		std::auto_ptr<ArgsArray> oArg = cp.GetContext().GetArguments();
		if(!oArg.get())
		{
			return op;
		}

		std::vector<std::wstring> arg_names;
		for(int i=0;i<oArg->m_vec_args.size()-1;++i)
		{
			if(oArg->m_vec_args[i].GetType()!=String || oArg->m_vec_args[i].IsReference())
			{
				return op;
			}

			arg_names.push_back(oArg->m_vec_args[i].GetStringValue());
		}

		if(oArg->m_vec_args.back().GetType()!=String)
		{
			return op;
		}

		if(oArg->m_vec_args.back().IsReference())
		{
			cp.GetContext().GetMemory().Read(VariableNameAddress(oArg->m_vec_args.back().ToString()),oArg->m_vec_args.back());
		}

		try
		{
			HardRefHolder hrf;
			cp.GetContext().GetObjMgr()->AddRef(new DynamicFunction(arg_names,oArg->m_vec_args.back().GetStringValue()),hrf);
			op.SetValue(hrf);
		}
		catch(int)
		{
		}

		return op;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}
	
	virtual std::wstring GetStringForm()
	{
		return L"make_function(){}";
	}

private:
	std::vector<std::wstring> m_arg_names;
};

class StoredWindowsFunction : public AbstractFunction
{
public:
	StoredWindowsFunction(const std::wstring& dll_name
		,const std::wstring& func_name
		,const std::wstring& func_sig
		)
		:m_func_ptr(0)
		//,m_func_sig(func_sig)
	{
		HMODULE hm = ::LoadLibraryW(dll_name.c_str());
		if(hm)
		{
			m_func_ptr = ::GetProcAddress(hm,SimpleCharTypeExpand(func_name.c_str()).c_str());
		}

		if(!FunctionType::FunctionTypeFromSignature(func_sig.c_str(),m_oFt))
			throw EvaluatorException("invalid function signature");
	}

	virtual bool NeedsANewMemoryContext()
	{
		return false;
	}

	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}
	
	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;

		std::auto_ptr<ArgsArray> oArg = cp.GetContext().GetArguments();
		if(!oArg.get())
		{
			return op;
		}

		return this->Call(cp,oArg->m_vec_args,0);
	}

	virtual bool HandlesReferenceArguments()
	{
		return true;
	}

	SOperand Call(IContextProvider& cp,std::vector<SOperand>& vec_args,int begin_index)
	{
		SOperand op;
		if(!m_func_ptr)
			return op;

		bool bUnknownSig = false;

		bool bHasReturnValueTypeSpec = false;

		std::map<int,std::wstring> mp_arg_index_to_variablename;
		int arg_index = begin_index;
		int i=0;
		for(i=0;i<m_oFt.m_oArgTypes.size() && arg_index<vec_args.size();++i,++arg_index)
		{
			if(vec_args[arg_index].IsReference())
			{
				if((m_oFt.m_oArgTypes[i].m_mod & Ref_Modifier) == Ref_Modifier)
				{
					mp_arg_index_to_variablename[arg_index] = vec_args[arg_index].ToString();
				}

				cp.GetContext().GetMemory().Read(VariableNameAddress(vec_args[arg_index].ToString()),vec_args[arg_index]);
			}

			switch(m_oFt.m_oArgTypes[i].m_t)
			{
			case Int:
				vec_args[arg_index].SetValue(vec_args[arg_index].ToInt());
				break;
			case String:
				vec_args[arg_index].SetValue(vec_args[arg_index].ToString());
				break;
			case ObjRefType:
				{
					std::wcout << L"making a windows callable wrapper" << std::endl;
					HardRefHolder hrh;
					ObjectsManager* om = cp.GetContext().GetObjMgr();
					if(om && m_oFt.m_oArgTypes[i].m_p)
					{
						om->AddRef(new Wcw(*(m_oFt.m_oArgTypes[i].m_p),vec_args[arg_index].GetObjRef(),cp),hrh);
						vec_args[arg_index].SetValue(hrh);
					}
				}
				break;
			/*case L')':
				bHasReturnValueTypeSpec = true;
				break;
			default:
				bUnknownSig = true;
				break;
			*/}

			/*if(bHasReturnValueTypeSpec)
				break;

			if(bUnknownSig)
				break;*/
		}

		Types retType = m_oFt.m_returnType.m_t;
		/*if(bHasReturnValueTypeSpec || i<m_func_sig.size())
		{
			if(bHasReturnValueTypeSpec || (m_func_sig[i]==L')'?(++i,1):0)) 
			{
				if(i<m_func_sig.size())
				{
					if(m_func_sig[i]=='s')
					{
						retType = String;
					}
				}
			}
		}

		if(bUnknownSig)
			return op;*/

		op.SetValue(MakeTheCall(vec_args,begin_index));

		for(std::map<int,std::wstring>::const_iterator i_index_name = mp_arg_index_to_variablename.begin();
			i_index_name != mp_arg_index_to_variablename.end();
			++i_index_name)
		{
			cp.GetContext().GetMemory().Write(VariableNameAddress(i_index_name->second),vec_args[i_index_name->first]);
		}

		if(retType!=Int)
		{
			if(retType==String)
			{
				int nVal = op.GetIntValue();
				if(nVal)
				{
					op.SetValue(reinterpret_cast<const wchar_t*>(nVal));
				}
			}
		}

		return op;
	}

	virtual std::wstring GetStringForm()
	{
		return L"variable_function(...){}";
	}

private:
	int MakeTheCall(std::vector<SOperand>& vec_args,int begin_index)
	{
		void* func_ptr = m_func_ptr;
		std::vector<int> vec_ints;
		bool bUnknownArgType = false;
		for(int i=begin_index;i<vec_args.size();++i)
		{
			switch(vec_args[i].GetType())
			{
			case Int:
				vec_ints.push_back(vec_args[i].GetIntValue());
				break;
			case String:
				vec_ints.push_back(reinterpret_cast<int>(vec_args[i].GetStringValue().c_str()));
				break;
			case Bool:
				vec_ints.push_back(vec_args[i].GetBoolValue());
				break;
			case ObjRefType:
				{
					if(!vec_args[i].GetObjRef()->Value())
					{
						std::wcout << L"the ref type held inside the hard reference is null" << std::endl;
					}
					else
					{
						Wcw* p = dynamic_cast<Wcw*>(vec_args[i].GetObjRef()->Value());
						if(!p)
						{
							std::wcout << L"dynamic cast to Wcw failed" << std::endl;
						}
						else
						{
							std::wcout << L"pointer to function is got. value = 0x" << std::hex << (int)p->GetEntryPoint()
								<< std::endl;
							vec_ints.push_back((int)p->GetEntryPoint());
						}
					}
				}
				break;
			default:
				bUnknownArgType = true;
				break;
			}
		}

		if(bUnknownArgType)
			return 0;

		if(vec_ints.size()<0)
			return 0;

		int* int_array = vec_ints.size()>0?&vec_ints.front():0;
		int last_index = vec_ints.size() - 1;
 
		int nRet = 0;

		__asm
		{
			push ecx;
			push eax;
			push edx;
			cmp last_index,-1;
			je after_arg_push;
			mov edx,int_array;
			mov ecx, last_index;
loop1:		mov eax, dword ptr [edx + ecx*4];
			push eax;
			dec ecx;
			jns loop1;
after_arg_push:
			call func_ptr;
			mov nRet,eax;
			pop edx;
			pop eax;
			pop ecx;
		}

		return nRet;
	}

	std::vector<std::wstring> m_arg_names;
	void* m_func_ptr;
	//std::wstring m_func_sig;
	FunctionType m_oFt;
};

class WindowsFunctionMaker : public AbstractFunction
{
public:
	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}
	
	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		if(!cp.GetContext().GetObjMgr())
			return op;

		std::auto_ptr<ArgsArray> oArg = cp.GetContext().GetArguments();
		if(!oArg.get())
		{
			return op;
		}

		if(oArg->m_vec_args.size()<2)
		{
			return op;
		}

		if(oArg->m_vec_args[0].GetStringValue().empty())
			return op;

		if(oArg->m_vec_args[1].GetStringValue().empty())
			return op;

		std::wstring func_sig = oArg->m_vec_args.size()>2?oArg->m_vec_args[2].GetStringValue():L"";

		HardRefHolder hrf;
		cp.GetContext().GetObjMgr()->AddRef(new StoredWindowsFunction(oArg->m_vec_args[0].GetStringValue(),oArg->m_vec_args[1].GetStringValue(),func_sig),hrf);

		op.SetValue(hrf);

		return op;
	}

	virtual std::wstring GetStringForm()
	{
		return L"make_windows_function(dll_name,function_name,[func_sig]){}";
	}
private:
	std::vector<std::wstring> m_arg_names;
};

class WindowsFunctionCaller : public AbstractFunction
{
public:
	virtual const std::vector<std::wstring>& GetArgNames()
	{
		return m_arg_names;
	}
	
	virtual SOperand Execute(IContextProvider& cp)
	{
		SOperand op;
		if(!cp.GetContext().GetObjMgr())
			return op;

		std::auto_ptr<ArgsArray> oArg = cp.GetContext().GetArguments();
		if(!oArg.get())
		{
			return op;
		}

		if(oArg->m_vec_args.size()<3)
		{
			return op;
		}

		if(oArg->m_vec_args[0].GetStringValue().empty())
			return op;

		if(oArg->m_vec_args[1].GetStringValue().empty())
			return op;

		const std::wstring& func_sig = oArg->m_vec_args[2].GetStringValue();
		
		StoredWindowsFunction swf(oArg->m_vec_args[0].GetStringValue(),oArg->m_vec_args[1].GetStringValue(),func_sig);
		return swf.Call(cp,oArg->m_vec_args,3);
	}

	virtual std::wstring GetStringForm()
	{
		return L"cwf(dll_name,function_name,func_sig,args,...){}";
	}
private:
	std::vector<std::wstring> m_arg_names;
};

int _tmain(int argc, _TCHAR* argv[])
{	
	/*int oA[] = {1,2,3,56};
	int* int_array = &oA[0];
	int c = (sizeof(oA)/sizeof(oA[0]) - 1);
	
	__asm
	{
		push ecx;
		push eax;
		push edx;
		mov edx,int_array;
		mov ecx,c;
loop1:  mov eax,dword ptr [edx + ecx*4];
		mov eax,oA[ecx*4];
		dec ecx;
		jns loop1;
		pop edx;
		pop eax;
		pop ecx;
	}*/

	SmExpressionGrammer oGr;
	ObjectsManager obm;
	HardRefHolder hrh;
	obm.AddRef(new StringLenFunction(),hrh);

	HardRefHolder hrh_println;
	obm.AddRef(new PrintFunction(),hrh_println);

	HardRefHolder hrh_loop;
	obm.AddRef(new LoopFunction(),hrh_loop);

	HardRefHolder hrh_fm;
	obm.AddRef(new FunctionMaker(),hrh_fm);

	HardRefHolder hrh_cwf;
	obm.AddRef(new WindowsFunctionCaller(),hrh_cwf);

	HardRefHolder hrf_make_windows_function;
	obm.AddRef(new WindowsFunctionMaker(),hrf_make_windows_function);

	SimpleComputationContext compContext(obm);
	compContext.GetMemMap()[L"strlen"].SetValue(hrh);
	compContext.GetMemMap()[L"print"].SetValue(hrh_println);
	compContext.GetMemMap()[L"loop"].SetValue(hrh_loop);

	compContext.GetMemMap()[L"make_function"].SetValue(hrh_fm);
	compContext.GetMemMap()[L"cwf"].SetValue(hrh_cwf);
	compContext.GetMemMap()[L"make_windows_function"].SetValue(hrf_make_windows_function);


	while(true)
	{
		std::wstring wstr;
		ReadLine(std::wcin,wstr);
		if(wstr.empty())
			break;

		//boost::spirit::tree_parse_info::a;

		boost::spirit::tree_parse_info<const wchar_t*> info = boost::spirit::ast_parse(wstr.c_str(),oGr,boost::spirit::space_p);
		//boost::spirit::parse_info<const wchar_t*> info = boost::spirit::parse(wstr.c_str(),oGr,boost::spirit::space_p);

		if(info.full)
		{
			std::wcout << L"parsing successful" << std::endl;
			if(info.trees.size()>0)
			{
				//boost::spirit::tree_to_xml(std::cout,info.trees);
				//std::cout << typeid(info.trees[0]).name();

				SOperand result = Evaluate(compContext,info);
				if(result.IsReference())
				{
					compContext.GetMemory().Read(VariableNameAddress(result.ToString()),result);
				}
				std::wcout << L"result = " << result.ToString().c_str() << std::endl;
			}
			else
			{
				std::wcout << L"number of syntax not greater than 0; size = " << info.trees.size() << std::endl;
			}
		}
		else
		{
			std::wcout << L"parsing stopped at " << info.stop << std::endl;
			std::wcout << L"parsing failed" << std::endl;
		}
	}

	return 0;
}
