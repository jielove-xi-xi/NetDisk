class CQueue 
{
private:
    stack<int>In_stack,Out_stack;

    void in2out()
    {
        while(!In_stack.empty())
        {
            Out_stack.push(In_stack.top());
            In_stack.pop();
        }
    }

public:
    CQueue()
    {
    }
    
    void appendTail(int value) 
    {
        In_stack.push(value);
    }
    
    int deleteHead() 
    {
        if(Out_stack.empty())
        {
            if(In_stack.empty())
            {
                return -1;
            }
            in2out();
        }
        int value=Out_stack.top();
        Out_stack.pop();
        return value;
    }
};
