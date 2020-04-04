#ifndef __S3SELECT_FUNCTIONS__
#define __S3SELECT_FUNCTIONS__


#include "s3select_oper.h"

namespace s3selectEngine {

enum class s3select_func_En_t {ADD,SUM,MIN,MAX,COUNT,TO_INT,TO_FLOAT,SUBSTR};

class base_function 
{

protected:
    bool aggregate;

public:
    //TODO bool semantic() validate number of argument and type
    virtual bool operator()(std::vector<base_statement *> *args, variable *result) = 0;
    base_function() : aggregate(false) {}
    bool is_aggregate() { return aggregate == true; }
    virtual void get_aggregate_result(variable *) {}

    virtual ~base_function(){}
};

class s3select_functions : public __clt_allocator {

    private:
        
        std::map<std::string,s3select_func_En_t> m_functions_library;

        void build_library()
        {
            // s3select function-name (string) --> function Enum
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("add",s3select_func_En_t::ADD) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("sum",s3select_func_En_t::SUM) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("count",s3select_func_En_t::COUNT) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("min",s3select_func_En_t::MIN) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("max",s3select_func_En_t::MAX) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("int",s3select_func_En_t::TO_INT) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("float",s3select_func_En_t::TO_FLOAT) );
            m_functions_library.insert(std::pair<std::string,s3select_func_En_t>("substr",s3select_func_En_t::SUBSTR) );
        }

    public:

        s3select_functions()
        {
            build_library();
        }

    base_function * create(std::string fn_name);
};

class __function : public base_statement
{

private:
    std::vector<base_statement *> arguments;
    std::string name;
    base_function *m_func_impl;
    s3select_functions *m_s3select_functions;
    variable m_result;

    void _resolve_name()
    {
        if (m_func_impl)
            return;

        base_function *f = m_s3select_functions->create(name);
        if (!f)
            throw base_s3select_exception("function not found", base_s3select_exception::s3select_exp_en_t::FATAL); //should abort query
        m_func_impl = f;
    }

public:
    virtual void traverse_and_apply(scratch_area *sa,projection_alias *pa)
    {
        m_scratch = sa;
        m_aliases = pa;
        for (base_statement *ba : arguments)
        {
            ba->traverse_and_apply(sa,pa);
        }
    }

    virtual bool is_aggregate() // TODO under semantic flow
    {
        _resolve_name();

        return m_func_impl->is_aggregate();
    }

    virtual bool semantic() { return true; }

    __function(const char *fname, s3select_functions* s3f) : name(fname), m_func_impl(0),m_s3select_functions(s3f) {}

    virtual value & eval(){

        _resolve_name();

        if (is_last_call == false)
            (*m_func_impl)(&arguments, &m_result);
        else
            (*m_func_impl).get_aggregate_result(&m_result);

        return m_result.get_value();
    }



    virtual std::string  print(int ident) {return std::string(0);}

    void push_argument(base_statement *arg)
    {
        arguments.push_back(arg);
    }


    std::vector<base_statement *> get_arguments()
    {
        return arguments;
    }

    virtual ~__function() {arguments.clear();}
};



/*
    s3-select function defintions
*/
struct _fn_add : public base_function{

    value var_result;

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        std::vector<base_statement*>::iterator iter = args->begin();
        base_statement* x =  *iter;
        iter++;
        base_statement* y = *iter;

        var_result = x->eval() + y->eval();
        
        *result = var_result; 

        return true;
    }
};

struct _fn_sum : public base_function
{

    value sum;

    _fn_sum() : sum(0) { aggregate = true; }

    bool operator()(std::vector<base_statement *> *args, variable *result)
    {
        std::vector<base_statement *>::iterator iter = args->begin();
        base_statement *x = *iter;

        try
        {
            sum = sum + x->eval();
        }
        catch (base_s3select_exception &e)
        {
            std::cout << "illegal value for aggregation(sum). skipping." << std::endl;
            if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL)
                throw;
        }

        return true;
    }

    virtual void get_aggregate_result(variable *result) { *result = sum ;} 
};

struct _fn_count : public base_function{

    int64_t count;

    _fn_count():count(0){aggregate=true;}

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        count += 1;

        return true;
    }

    virtual void get_aggregate_result(variable*result){ result->set_value(count);}
    
};

struct _fn_min : public base_function{

    value min;

    _fn_min():min(__INT64_MAX__){aggregate=true;}

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        std::vector<base_statement*>::iterator iter = args->begin();
        base_statement* x =  *iter;

        if(min > x->eval()) min=x->eval();

        return true;
    }

    virtual void get_aggregate_result(variable*result){ *result = min;}
    
};

struct _fn_max : public base_function{

    value max;

    _fn_max():max(-__INT64_MAX__){aggregate=true;}

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        std::vector<base_statement*>::iterator iter = args->begin();
        base_statement* x =  *iter;

        if(max < x->eval()) max=x->eval();

        return true;
    }

    virtual void get_aggregate_result(variable*result){*result = max;}
    
};

struct _fn_to_int : public base_function{

    value var_result;
    value func_arg;

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        char *perr;
        int64_t i=0;
        func_arg = (*args->begin())->eval();

        if (func_arg.type == value::value_En_t::STRING)
                i = strtol(func_arg.str() ,&perr ,10) ;//TODO check error before constructor
        else
        if (func_arg.type == value::value_En_t::FLOAT)
                i = func_arg.dbl();
        else
                i = func_arg.i64();
        
        var_result =  i ;
        *result =  var_result;

        return true;
    }
    
};

struct _fn_to_float : public base_function{

    value var_result;
    value v_from;

    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        char *perr;
        double d=0;
        value v = (*args->begin())->eval();

        if (v.type == value::value_En_t::STRING)
                d = strtod(v.str() ,&perr) ;//TODO check error before constructor
        else
        if (v.type == value::value_En_t::FLOAT)
                d = v.dbl();
        else
                d = v.i64();
        
        var_result = d;
        *result = var_result;

        return true;
    }
    
};

struct _fn_substr : public base_function{

    char buff[4096];// this buffer is persist for the query life time, it use for the results per row(only for the specific function call)
    //it prevent from intensive use of malloc/free (fragmentation).
    //should validate result length.
    //TODO may replace by std::string (dynamic) , or to replace with global allocator , in query scope.
    value v_str;
    value v_from;
    value v_to;
    
    bool operator()(std::vector<base_statement*> * args,variable * result)
    {
        std::vector<base_statement*>::iterator iter = args->begin();
        int args_size = args->size();


        if (args_size<2)
            throw base_s3select_exception("substr accept 2 arguments or 3");

        base_statement* str =  *iter;
        iter++;
        base_statement* from = *iter;
        base_statement* to;

        if (args_size == 3)
                {
            iter++;
            to = *iter;
        }

        v_str = str->eval();

        if(v_str.type != value::value_En_t::STRING)
            throw base_s3select_exception("substr first argument must be string");//can skip current row

        int str_length = strlen(v_str.str());

        v_from = from->eval();
        if(v_from.is_string())
                    throw base_s3select_exception("substr second argument must be number");//can skip current row

        int64_t f;
        int64_t t;

        if (args_size==3){
            v_to = to->eval();
            if (v_to.is_string())
                throw base_s3select_exception("substr third argument must be number");//can skip row
        }
        
        if (v_from.type == value::value_En_t::FLOAT)
            f=v_from.dbl();
        else
            f=v_from.i64();

        if (f>str_length)
            throw base_s3select_exception("substr start position is too far");//can skip row

        if (str_length>(int)sizeof(buff))
            throw base_s3select_exception("string too long for internal buffer");//can skip row

        if (args_size == 3)
        {
            if (v_from.type == value::value_En_t::FLOAT)
                t = v_to.dbl();
            else
                t = v_to.i64();

            if( (str_length-(f-1)-t) <0)
                throw base_s3select_exception("substr length parameter beyond bounderies");//can skip row

            strncpy(buff,v_str.str()+f-1,t);
        }
        else 
            strcpy(buff,v_str.str()+f-1);
        
        result->set_value(buff);

        return true;
    }
    
};

base_function *s3select_functions::create(std::string fn_name)
{
    std::map<std::string, s3select_func_En_t>::iterator iter = m_functions_library.find(fn_name);

    if (iter == m_functions_library.end())
    {
        std::string msg;
        msg = fn_name + " " + " function not found";
        throw base_s3select_exception(msg, base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    switch (iter->second)
    {
    case s3select_func_En_t::ADD:
        return S3SELECT_NEW(_fn_add);
        break;

    case s3select_func_En_t::SUM:
        return S3SELECT_NEW(_fn_sum);
        break;

    case s3select_func_En_t::COUNT:
        return S3SELECT_NEW(_fn_count);
        break;

    case s3select_func_En_t::MIN:
        return S3SELECT_NEW(_fn_min);
        break;

    case s3select_func_En_t::MAX:
        return S3SELECT_NEW(_fn_max);
        break;

    case s3select_func_En_t::TO_INT:
        return S3SELECT_NEW(_fn_to_int);
        break;

    case s3select_func_En_t::TO_FLOAT:
        return S3SELECT_NEW(_fn_to_float);
        break;

    case s3select_func_En_t::SUBSTR:
        return S3SELECT_NEW(_fn_substr);
        break;

    default:
        throw base_s3select_exception("internal error while resolving function-name");
        break;
    }
}

bool base_statement::is_function()
{
    if (dynamic_cast<__function *>(this))
        return true;
    else
        return false;
}

bool base_statement::is_aggregate_exist_in_expression(base_statement *e) //TODO obsolete ?
{
    if (e->is_aggregate())
        return true;

    if (e->left() && e->left()->is_aggregate_exist_in_expression(e->left()))
        return true;

    if (e->right() && e->right()->is_aggregate_exist_in_expression(e->right()))
        return true;

    if (e->is_function())
    {
        for (auto i : dynamic_cast<__function *>(e)->get_arguments())
            if (e->is_aggregate_exist_in_expression(i))
                return true;
    }

    return false;
}

base_statement *base_statement::get_aggregate()
{//search for aggregation function in AST
    base_statement * res = 0;

    if (is_aggregate())
        return this;

    if (left() && (res=left()->get_aggregate())!=0) return res;

    if (right() && (res=right()->get_aggregate())!=0) return res;

    if (is_function())
    {
        for (auto i : dynamic_cast<__function *>(this)->get_arguments())
        {
            base_statement* b=i->get_aggregate();
            if (b) return b;
        }
    }
    return 0;
}

bool base_statement::is_nested_aggregate(base_statement *e) 
{//validate for non nested calls for aggregation function, i.e. sum ( min ( ))
    if (e->is_aggregate())
    {
        if (e->left())
        {
            if (e->left()->is_aggregate_exist_in_expression(e->left()))
                return true;
        }
        else if (e->right())
        {
            if (e->right()->is_aggregate_exist_in_expression(e->right()))
                return true;
        }
        else if (e->is_function())
        {
            for (auto i : dynamic_cast<__function *>(e)->get_arguments())
            {
                if (i->is_aggregate_exist_in_expression(i)) return true;
            }
        }
        return false;
    }
    return false;
}

// select sum(c2) ... + c1 ... is not allowed. a binary operation with scalar is OK. i.e. select sum() + 1
bool base_statement::is_binop_aggregate_and_column(base_statement *skip_expression)
{
    if (left() && left() != skip_expression) //can traverse to left
    {
        if (left()->is_column())
            return true;
        else
            if (left()->is_binop_aggregate_and_column(skip_expression) == true) return true;
    }
    
    if (right() && right() != skip_expression) //can traverse right
    {
        if (right()->is_column())
            return true;
        else
            if (right()->is_binop_aggregate_and_column(skip_expression) == true) return true;
    }

    if (this != skip_expression && is_function())
    {

        __function* f = (dynamic_cast<__function *>(this));
        std::vector<base_statement*> l = f->get_arguments();
        for (auto i : l)
        {
            if (i!=skip_expression && i->is_column())
                return true;
            if (i->is_binop_aggregate_and_column(skip_expression) == true) return true;
        }
    }

    return false;
}

} //namespace s3selectEngine

#endif