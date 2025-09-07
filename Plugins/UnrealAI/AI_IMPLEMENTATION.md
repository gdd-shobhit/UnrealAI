# UnrealAI Plugin - AI Implementation Guide

## Overview

The UnrealAI plugin now includes comprehensive AI integration with support for:
- **Local LLMs** (Ollama, LM Studio, etc.)
- **Claude API** (Anthropic)
- **OpenAI API** (GPT-4, GPT-3.5)

## Features

### 🤖 AI Providers
- **Local LLM**: Run AI models locally for privacy and speed
- **Claude API**: Access to Claude 3 Sonnet for advanced reasoning
- **OpenAI API**: Access to GPT-4 and GPT-3.5 models

### 🎯 AI Capabilities
- **Blueprint Generation**: Generate Blueprint logic from descriptions
- **C++ Code Generation**: Generate C++ classes and functions
- **Code Review**: Get AI-powered code analysis and suggestions
- **Documentation**: Generate documentation for your code
- **General Queries**: Ask questions about Unreal Engine development

## Setup Instructions

### 1. Local LLM Setup (Recommended)

#### Option A: Ollama (Easiest)
1. **Install Ollama**: Download from [ollama.ai](https://ollama.ai)
2. **Install a Model**: Open terminal and run:
   ```bash
   ollama pull llama2
   # or for better performance:
   ollama pull llama2:13b
   ```
3. **Start Ollama**: The service runs automatically on `http://localhost:11434`
4. **Configure Plugin**: 
   - Open Project Settings → Plugins → UnrealAI
   - Set Local LLM Endpoint to: `http://localhost:11434/api/generate`
   - Enable Local LLM

#### Option B: LM Studio
1. **Install LM Studio**: Download from [lmstudio.ai](https://lmstudio.ai)
2. **Download a Model**: Choose a model like Llama 2 or Mistral
3. **Start Local Server**: 
   - Go to Local Server tab
   - Click "Start Server"
   - Note the endpoint URL
4. **Configure Plugin**: Use the LM Studio endpoint URL

### 2. Claude API Setup

1. **Get API Key**: 
   - Sign up at [console.anthropic.com](https://console.anthropic.com)
   - Create an API key
2. **Configure Plugin**:
   - Open Project Settings → Plugins → UnrealAI
   - Enable Claude API
   - Enter your API key (starts with `sk-ant-`)

### 3. OpenAI API Setup

1. **Get API Key**:
   - Sign up at [platform.openai.com](https://platform.openai.com)
   - Create an API key
2. **Configure Plugin**:
   - Open Project Settings → Plugins → UnrealAI
   - Enable OpenAI API
   - Enter your API key (starts with `sk-`)

## Usage

### Using the Plugin Interface

1. **Open the Plugin**:
   - Click the "UnrealAI" button in the toolbar
   - Or go to Window → UnrealAI

2. **Choose Request Type**:
   - **General Query**: Ask any Unreal Engine question
   - **Blueprint Generation**: Generate Blueprint logic
   - **C++ Generation**: Generate C++ code
   - **Code Review**: Review existing code
   - **Documentation**: Generate documentation

3. **Select AI Provider**:
   - **Local LLM**: Fast, private, no internet required
   - **Claude API**: Best reasoning, requires internet
   - **OpenAI API**: Good balance, requires internet

4. **Enter Your Prompt**:
   - Be specific about what you want
   - Include context about your project
   - Mention any specific requirements

### Example Prompts

#### Blueprint Generation
```
Create a Blueprint for a health system that:
- Has max health of 100
- Takes damage from enemy attacks
- Regenerates 5 health per second when not taking damage
- Shows health bar UI
- Plays sound effects for damage and healing
```

#### C++ Generation
```
Create a C++ class for a weapon system that:
- Inherits from AActor
- Has damage, fire rate, and ammo properties
- Implements shooting mechanics
- Has reload functionality
- Uses UPROPERTY macros for Blueprint access
```

#### Code Review
```
Review this C++ code for best practices and potential issues:
[Paste your code here]
```

## Configuration Options

### General Settings
- **Default Temperature**: Controls randomness (0.0 = deterministic, 1.0 = very random)
- **Default Max Tokens**: Maximum response length
- **Preferred Provider**: Default AI provider to use
- **Debug Logging**: Enable detailed logging

### Blueprint Generation Settings
- **Include Best Practices**: Add optimization tips
- **Include Performance Tips**: Add performance advice
- **Include Comments**: Add explanatory comments

### C++ Generation Settings
- **Include Header Comments**: Add documentation to headers
- **Include Implementation Comments**: Add inline comments
- **Include Memory Management Tips**: Add memory safety advice

### Code Review Settings
- **Include Security Analysis**: Check for security issues
- **Include Performance Analysis**: Suggest optimizations
- **Include Best Practices Review**: Check coding standards

## Troubleshooting

### Local LLM Issues
- **Connection Failed**: Check if Ollama/LM Studio is running
- **Slow Responses**: Try a smaller model or better hardware
- **Model Not Found**: Download the model first (`ollama pull model_name`)

### API Issues
- **Authentication Failed**: Check your API key
- **Rate Limited**: Wait and try again, or upgrade your plan
- **Network Error**: Check your internet connection

### Plugin Issues
- **Not Loading**: Restart Unreal Editor
- **Settings Not Saving**: Check file permissions
- **Compilation Errors**: Rebuild the plugin

## Advanced Usage

### Custom Prompts
You can customize the AI prompts by modifying the prompt building functions in `UnrealAIService.cpp`:

- `BuildBlueprintPrompt()`: Customize Blueprint generation prompts
- `BuildCPPPrompt()`: Customize C++ generation prompts
- `BuildCodeReviewPrompt()`: Customize code review prompts

### Adding New AI Providers
To add a new AI provider:

1. Add the provider to `EAIProvider` enum
2. Implement `Process[Provider]Request()` function
3. Add configuration options to `UUnrealAISettings`
4. Update the UI to include the new provider

### Integration with Blueprint Editor
The plugin can be extended to integrate directly with the Blueprint editor for:
- Auto-completion suggestions
- Node generation from text descriptions
- Code review of Blueprint logic

## Performance Tips

### Local LLM Optimization
- Use smaller models for faster responses
- Close other applications to free up RAM
- Use GPU acceleration if available
- Consider using quantized models (e.g., `llama2:7b-q4_0`)

### API Usage Optimization
- Cache common responses
- Use streaming for long responses
- Implement retry logic for failed requests
- Monitor API usage and costs

## Security Considerations

### Local LLM
- ✅ No data leaves your machine
- ✅ No API costs
- ✅ Works offline
- ⚠️ Requires local resources

### Cloud APIs
- ⚠️ Data sent to third parties
- ⚠️ Potential costs
- ⚠️ Requires internet
- ✅ No local resource usage

## Future Enhancements

- **Streaming Responses**: Real-time response streaming
- **Context Awareness**: AI that understands your project structure
- **Auto-Implementation**: Direct code generation in editor
- **Learning Mode**: AI that learns from your coding style
- **Team Collaboration**: Shared AI configurations and prompts

## Support

For issues and questions:
1. Check the troubleshooting section above
2. Review the Unreal Engine logs
3. Test with different AI providers
4. Verify your configuration settings

## License

This plugin is provided as-is for educational and development purposes. Please respect the terms of service for any AI providers you use.
