# UnrealAI Plugin

A standalone Unreal Engine plugin for AI-powered Blueprint and C++ code generation, code review, and development assistance.

## Current Status ✅

The plugin has been successfully compiled and is ready for testing! Here's what's currently working:

### ✅ Completed Features
- **Plugin Structure**: Complete plugin architecture with proper module setup
- **AI Service**: Core AI service class with request/response handling
- **Blueprint Integration**: All AI functions are exposed to Blueprints
- **C++ Integration**: Full C++ API for AI functionality
- **Test Interface**: Working toolbar button with test functionality
- **Enum Definitions**: AI providers (Local LLM, Claude, OpenAI) and request types
- **Response System**: Structured AI response handling with delegates

### 🔧 Current Implementation
- **Test Mode**: Plugin currently returns test responses (HTTP functionality disabled)
- **Basic UI**: Simple dialog-based interface for testing
- **Service Architecture**: Complete AI service with provider abstraction
- **Blueprint Support**: All functions callable from Blueprints

## How to Test

1. **Open Unreal Editor** with your project
2. **Look for the UnrealAI button** in the toolbar (should appear automatically)
3. **Click the button** to see the test interface
4. **Test the AI service** by clicking "OK" in the dialog

## Available AI Functions

### Blueprint Generation
```cpp
// Generate Blueprint from description
AIService->GenerateBlueprint("Create a player character that can jump and shoot");
```

### C++ Generation
```cpp
// Generate C++ code from description
AIService->GenerateCPPCode("Create a custom actor class with health system");
```

### Code Review
```cpp
// Review existing code
AIService->ReviewCode("Your C++ code here", "cpp");
```

### General AI Requests
```cpp
// Send custom AI requests
FAIRequest Request;
Request.Prompt = "Your question here";
Request.RequestType = EAIRequestType::General;
Request.Provider = EAIProvider::LocalLLM;
AIService->SendAIRequest(Request);
```

## Next Steps

### 1. Enable Real AI Responses
To get actual AI responses instead of test responses:

1. **Install Ollama**: Download from https://ollama.ai
2. **Pull a model**: `ollama pull llama2`
3. **Start Ollama**: `ollama serve`
4. **Enable HTTP**: Uncomment HTTP functionality in the code

### 2. Configure API Keys
For Claude and OpenAI support:

1. **Get API keys** from respective services
2. **Configure endpoints** in the plugin settings
3. **Enable providers** in the configuration

### 3. Advanced UI
- Create a full UMG-based interface
- Add real-time response streaming
- Implement code highlighting and formatting

## File Structure

```
Plugins/UnrealAI/
├── Source/UnrealAI/
│   ├── UnrealAI.h/cpp          # Main module
│   ├── UnrealAICommands.h/cpp  # UI commands
│   ├── UnrealAIService.h/cpp   # Core AI service
│   ├── UnrealAIWidget.h/cpp    # UMG widget (future)
│   └── UnrealAISimpleWidget.h/cpp # Test widget
├── UnrealAI.uplugin           # Plugin descriptor
└── README.md                  # This file
```

## Troubleshooting

### Plugin Not Appearing
- Ensure the plugin is enabled in Project Settings
- Check that the build completed successfully
- Restart the Unreal Editor

### Compilation Errors
- Make sure you're using Unreal Engine 5.5
- Check that all required modules are included
- Verify the plugin is properly added to the project

### Test Responses Only
- This is expected behavior until HTTP functionality is enabled
- The plugin is working correctly if you see test responses
- Follow the "Next Steps" section to enable real AI

## Development Notes

- **HTTP Module**: Currently disabled to avoid compilation issues
- **Settings System**: Temporarily disabled, will be re-enabled later
- **UI System**: Basic dialog interface, full UMG interface planned
- **Error Handling**: Basic error handling implemented
- **Extensibility**: Easy to add new AI providers and request types

## License

This plugin is provided as-is for development and testing purposes.
