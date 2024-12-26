#include "Syntax.hpp"
#include "Environment.hpp"

namespace Sysmel
{
ValuePtr SyntaxBinaryExpressionSequence::analyzeInEnvironment(const EnvironmentPtr &environment)
{
    auto result = elements[0]->analyzeInEnvironment(environment);
    for (size_t i = 1; i < elements.size(); i += 2)
    {
        auto operatorSymbol = elements[i];
        auto operand = elements[i + 1];

        auto messageSend = std::make_shared<SyntaxMessageSend> ();
        messageSend->sourcePosition = result->getSourcePosition()->to(operand->getSourcePosition());
        messageSend->receiver = result;
        messageSend->selector = operatorSymbol;
        messageSend->arguments.push_back(operand);
        result = messageSend->analyzeInEnvironment(environment);

    }
    return result;
}

ValuePtr SyntaxMessageSend::analyzeInEnvironment(const EnvironmentPtr &environment)
{
    auto analyzedSelector = selector->analyzeInEnvironment(environment);
    auto analyzedSelectorSymbol = analyzedSelector->asAnalyzedSymbolValue();
    if(!receiver && analyzedSelectorSymbol)
    {
        auto functional = environment->lookupSymbolRecursively(analyzedSelectorSymbol);
        abort();
    }
    
    auto analyzedReceiver = receiver->analyzeInEnvironment(environment);
    auto receiverTypeOrClass = analyzedReceiver->getTypeOrClass();
    
    std::vector<ValuePtr> analyzedArguments;
    analyzedArguments.reserve(arguments.size());
    for(auto argument : arguments)
    {
        analyzedArguments.push_back(argument->analyzeInEnvironment(environment));
    }

    abort();
}
}