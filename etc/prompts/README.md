# Predefined Prompts for AI Log Analysis

This directory contains predefined prompt templates for AI-powered log analysis.

## How to Use

1. In the AI Analysis panel, check "Use Custom Prompt"
2. Select a template from the "Prompt Template" dropdown
3. The prompt will be loaded into the custom prompt text area
4. You can edit the prompt before running the analysis
5. Click "Analyze Logs" to run the analysis with the selected prompt

## Creating Custom Prompts

You can create your own prompt templates by adding text files to this directory:

- Supported file extensions: `.txt`, `.prompt`
- Files are automatically detected and loaded on application start
- File name (without extension) becomes the template name in the dropdown

### Example Prompt File

Create a file named `my_analysis.txt`:

```
Analyze the logs for [specific issue].
Look for:
- Point 1
- Point 2
- Point 3

Provide recommendations for fixing the issues found.
```

## Included Prompts

- **performance_analysis.txt** - Identify performance bottlenecks and slow operations
- **security_audit.txt** - Security-focused analysis for vulnerabilities and threats
- **database_issues.txt** - Database connection, query, and transaction problems
- **network_diagnostics.txt** - Network connectivity and protocol issues
- **crash_analysis.txt** - Application crashes and fatal errors
- **user_experience.txt** - User-facing issues and UX problems

## Tips for Writing Good Prompts

1. **Be Specific**: Clearly define what you want the AI to look for
2. **Use Structure**: Use bullet points or numbered lists for clarity
3. **Request Details**: Ask for timestamps, line numbers, or specific examples
4. **Ask for Recommendations**: Request actionable suggestions for fixes
5. **Set Context**: Explain what type of analysis you need
6. **Keep it Concise**: Avoid overly long prompts that might confuse the AI

## Loading Prompts from Files

You can also load prompts from any location using the "Load from File..." button.
This is useful for:
- One-time custom analyses
- Sharing prompts between team members
- Testing new prompt templates before adding them to this directory
