#include "Syntax.hpp"
#include "Environment.hpp"
#include "Type.hpp"

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
        auto functionalIdentifier = std::make_shared<SyntaxIdentifierReference> ();
        functionalIdentifier->sourcePosition = analyzedSelector->sourcePosition;
        functionalIdentifier->value = analyzedSelectorSymbol->value;

        auto functionApplication = std::make_shared<SyntaxApplication>();
        functionApplication->functional = functionalIdentifier;
        functionApplication->sourcePosition = sourcePosition;
        functionApplication->arguments = arguments;
        return functionApplication->analyzeInEnvironment(environment);
    }
    
    auto analyzedReceiver = receiver->analyzeInEnvironment(environment);
    auto receiverTypeOrClass = analyzedReceiver->getTypeOrClass()->asTypeValue();

    auto method = receiverTypeOrClass->lookupSelector(analyzedSelectorSymbol);
    if(method)
    {
        auto methodType = method->getType()->asTypeValue(); 
        auto argContext = methodType->createArgumentTypeAnalysisContext();

        std::vector<ValuePtr> analyzedArguments;
        analyzedArguments.reserve(arguments.size());

        analyzedReceiver = argContext->coerceArgumentWithIndex(0, analyzedReceiver);
        for(size_t i = 0; i < arguments.size(); ++i)
        {
            auto analyzedArgument = arguments[i]->analyzeInEnvironment(environment);
            analyzedArgument = argContext->coerceArgumentWithIndex(i + 1, analyzedArgument);
            analyzedArguments.push_back(analyzedArgument);

        }

        auto analyzedMessage = std::make_shared<SemanticMessageSend> ();
        analyzedMessage->sourcePosition = sourcePosition;
        analyzedMessage->receiver = analyzedReceiver;
        analyzedMessage->selector = analyzedSelectorSymbol;
        analyzedMessage->arguments.swap(analyzedArguments);
        analyzedMessage->type = argContext->getResultType();
        return analyzedMessage;
    }
    else if(receiverTypeOrClass->isClass())
    {
        abort();
    }
    else
    {
        throwExceptionWithMessageAt(("There is no method with selector " + analyzedSelectorSymbol->printString() + " in the value.").c_str(), sourcePosition);
    }

    abort();
/*    std::vector<ValuePtr> analyzedArguments;
    analyzedArguments.reserve(1 + arguments.size());
    analyzedArguments.push

    for(size_t i = 1; i < analyzedArguments.size(); ++i)
    {
        analyzedArguments.push_back(argument->analyzeInEnvironment(environment));
    }

    auto analyzedMessage = std::make_shared<SemanticMessageSend> ();
    analyzedMessage->sourcePosition = sourcePosition;
    analyzedMessage->receiver = analyzedReceiver;
    analyzedMessage->selector = analyzedSelectorSymbol;
    analyzedMessage->arguments.swap(analyzedArguments);


    analyzedMessage->type = GradualType::uniqueInstance();

    return analyzedMessage;
*/
}

ValuePtr SyntaxMessageCascade::analyzeInEnvironment(const EnvironmentPtr &environment)
{
    auto analyzedReceiver = receiver->analyzeInEnvironment(environment);
    if(messages.empty())
        return analyzedReceiver;

    auto sequence = std::make_shared<SemanticValueSequence> ();
    sequence->sourcePosition = sourcePosition;
    sequence->elements.reserve(1 + messages.size());
    sequence->elements.push_back(analyzedReceiver);

    for (auto &message : messages)
    {
        auto cascadeMessage = std::static_pointer_cast<SyntaxMessageCascadeMessage> (message);
        auto nonCascadedMessage = cascadeMessage->asMessageSendWithReceiver(analyzedReceiver);
        auto analyzedMessage = nonCascadedMessage->analyzeInEnvironment(environment);
        sequence->elements.push_back(analyzedMessage);
    }

    return sequence;
}

SyntaxMessageSendPtr SyntaxMessageCascadeMessage::asMessageSendWithReceiver(const ValuePtr &receiver)
{
    auto messageSend = std::make_shared<SyntaxMessageSend> ();
    messageSend->sourcePosition = sourcePosition;
    messageSend->receiver = receiver;
    messageSend->selector = selector;
    messageSend->arguments = arguments;
    return messageSend;
}

ValuePtr SyntaxAssignment::analyzeInEnvironment(const EnvironmentPtr &environment)
{
    auto expandedStore = store->analyzeInEnvironmentForMacroExpansionOnly(environment);
    if (expandedStore->isFunctionalDependentTypeNode())
    {
        auto function = std::make_shared<SyntaxFunction> ();
        function->sourcePosition = sourcePosition;
        function->functionalType = std::static_pointer_cast<SyntaxFunctionalDependentType> (expandedStore);
        function->body = value;
        return function->analyzeInEnvironment(environment);
    } 
    else if (expandedStore->isBindableName())
    {
        auto bindableName = std::static_pointer_cast<SyntaxBindableName>(expandedStore);
        if(bindableName->typeExpression && bindableName->typeExpression->isFunctionalDependentTypeNode())
        {
            auto function = std::make_shared<SyntaxFunction> ();
            function->sourcePosition = sourcePosition;
            function->nameExpression = bindableName->nameExpression;
            function->functionalType = std::static_pointer_cast<SyntaxFunctionalDependentType> (bindableName->typeExpression);
            function->body = value;
            function->isFixpoint = bindableName->hasPostTypeExpression;

            auto bindingDefinition = std::make_shared<SyntaxBindingDefinition> ();
            bindingDefinition->nameExpression = bindableName->nameExpression;
            bindingDefinition->expectedTypeExpression = nullptr;
            bindingDefinition->initialValueExpression = function;
            bindingDefinition->isMutable = bindableName->isMutable;
            bindingDefinition->isPublic = false;
            bindingDefinition->isRebind = false;
            return bindingDefinition->analyzeInEnvironment(environment);
        }
        else
        {
            auto bindPattern = std::make_shared<SyntaxBindPattern> ();
            bindPattern->sourcePosition = sourcePosition;
            bindPattern->pattern = expandedStore;
            bindPattern->value = value;
            bindPattern->allowsRebind = false;
            return bindPattern->analyzeInEnvironment(environment);
        }
    }
    else
    {
        auto analyzedStore = expandedStore->analyzeInEnvironment(environment);
        auto storeType = analyzedStore->getType();
        if(!storeType->isReferenceLikeType())
            throwExceptionWithMessage("Assignment requires a reference, or something like that.");
        auto baseType = storeType->getDecayedType();
        
        auto analyzedValue = value->analyzeInEnvironment(environment);
        analyzedValue = analyzedValue->coerceIntoExpectedTypeAt(baseType, sourcePosition);

        auto semanticAssignment = std::make_shared<SemanticReferenceAssignment> ();
        semanticAssignment->sourcePosition = sourcePosition;
        semanticAssignment->reference = analyzedStore;
        semanticAssignment->value = analyzedValue;
        semanticAssignment->type = baseType;
        //printf("analyzedStore %s | %s\n", analyzedStore->printString().c_str(), baseType->printString().c_str());
        //printf("analyzedValue %s\n", analyzedValue->printString().c_str());
        return semanticAssignment;
    }
}

ValuePtr SyntaxBindableName::expandBindingOfValueWithAt(const ValuePtr &value, const SourcePositionPtr &position)
{
    auto binding = std::make_shared<SyntaxBindingDefinition> ();
    binding->sourcePosition = position;
    binding->nameExpression = nameExpression;
    binding->expectedTypeExpression = typeExpression;
    binding->initialValueExpression = value;
    binding->isMutable = this->isMutable;
    binding->isPublic = this->isPublic;
    return binding;
}

}