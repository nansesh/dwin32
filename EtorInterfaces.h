#pragma once
#include <boost/typeof/typeof.hpp>

#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost\preprocessor\punctuation\comma_if.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>
#include <boost/preprocessor/list/cat.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/list/for_each.hpp>

#define SHIFT_TO_SIZE(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N

#define ARGS(...)	(__VA_ARGS__,8,7,6,5,4,3,2,1)

#define VA_ARGS_SIZE(...)	BOOST_PP_EXPAND(SHIFT_TO_SIZE ARGS(__VA_ARGS__) )

#define SIZE_AND_ARGS(...)	(VA_ARGS_SIZE(__VA_ARGS__), ( __VA_ARGS__ ))

#define CLASS_NAME_LIST(...)		BOOST_PP_EXPAND ( BOOST_PP_TUPLE_TO_LIST SIZE_AND_ARGS(__VA_ARGS__) )

#define CLASS_NAME(...)		BOOST_PP_LIST_CAT( CLASS_NAME_LIST ( Riia , __VA_ARGS__) )

#define ARG_LIST_PRODUCER(r, data, elem)	BOOST_TYPEOF(elem) & BOOST_PP_CAT(elem,_name),

#define MEM_LIST_PRODUCER(r, data, elem)	BOOST_TYPEOF(elem) & elem;

#define INITIALIZER_LIST_PRODUCER(r, data, elem)	elem ( BOOST_PP_CAT(elem,_name) ),

/*
Given __VA_ARGS__ 
1. Find length of __VA_ARGS__ using the argument shifting trick
2. Use that size and call BOOST_PP_TUPLE_TO_LIST(size, ( __VA_ARGS__))
3. Use that list as an argument to call BOOST_PP_LIST_CAT(list)
4. Use BOOST_PP_LIST_FOR_EACH(macro, data, list) to make constructor argument list, initializer list, and member declaration list.
*/

#define RIIA_CLASS(acquire_code,release_code,...)	class CLASS_NAME(__VA_ARGS__) { \
public: explicit CLASS_NAME(__VA_ARGS__)\
			(\
	BOOST_PP_LIST_FOR_EACH ( ARG_LIST_PRODUCER, _ , CLASS_NAME_LIST(__VA_ARGS__) ) void* ignored = 0\
	):\
		BOOST_PP_LIST_FOR_EACH ( INITIALIZER_LIST_PRODUCER, _ , CLASS_NAME_LIST(__VA_ARGS__) ) _ignored(0) \
		{ \
			acquire_code ; \
		} \
		~ CLASS_NAME(__VA_ARGS__) \
		() { \
		release_code ; \
		} \
\
		BOOST_PP_LIST_FOR_EACH ( MEM_LIST_PRODUCER, _ , CLASS_NAME_LIST(__VA_ARGS__) ) void* _ignored; \
	}; CLASS_NAME(__VA_ARGS__) BOOST_PP_CAT(_obj_ , CLASS_NAME(__VA_ARGS__) ) ( __VA_ARGS__ );

#define RIIA_GEN_HOLDER(acquire,release,...)	RIIA_CLASS(acquire,release,__VA_ARGS__)

class Context;

class IContextProvider
{
public:
	virtual Context& GetContext() = 0;
};

class AbstractFunction : public RefType
{
public:
	virtual SOperand Execute(IContextProvider& cp) = 0;
	virtual const std::vector<std::wstring>& GetArgNames() = 0;
	virtual bool HandlesReferenceArguments()
	{
		return false;
	}

	virtual bool NeedsANewMemoryContext()
	{
		return true;
	}
};

class EvaluatorException:public std::runtime_error
{
public:
	explicit EvaluatorException(const std::string& message):std::runtime_error(message)
	{}
};

class MemoryException : public EvaluatorException
{
public:
	explicit MemoryException(const std::string& message):EvaluatorException(message)
	{}
};

class Address
{
public:
	virtual std::wstring ToString() const = 0;
};

class Memory
{
public:
	virtual void Read(const Address&,SOperand&) = 0;
	virtual void Write(const Address&, const SOperand&) = 0;
	virtual ~Memory()
	{}
};

class ArgsArray
{
public:
	ArgsArray(std::vector<SOperand>& vec_args)
		:m_vec_args(vec_args)
	{
	}

	std::vector<SOperand>& Args()
	{
		return m_vec_args;
	}

	std::vector<SOperand>& m_vec_args;
};

class Context
{
public:
	virtual Memory& GetMemory() = 0;
	virtual void EnterACallContext(const std::vector<std::wstring>& arg_names,std::vector<SOperand>& arg_values,bool bNeedNewNamespace) = 0;
	virtual void LeaveCallContext() = 0;
	virtual std::auto_ptr<ArgsArray> GetArguments() = 0;
	virtual ObjectsManager* GetObjMgr() = 0;
};
