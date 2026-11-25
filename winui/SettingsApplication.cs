using System.Text;
using System.Text.Json;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;

namespace BaCafe.Settings;

public sealed partial class SettingsApplication : Application
{
    private readonly StreamWriter output = new(Console.OpenStandardOutput(), new UTF8Encoding(false)) { AutoFlush = true };
    private IReadOnlyList<Field> fields = [];
    private MainWindow? full;
    private QuickWindow? quick;
    private JsonElement state;
    private string status = "", sampling = "";
    private NativeWindow.Point lastPosition;
    private bool exiting;

    internal SettingsApplication()
    {
        InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var dispatcher = DispatcherQueue.GetForCurrentThread();
        _ = Task.Run(async () =>
        {
            using var input = new StreamReader(Console.OpenStandardInput(), Encoding.UTF8);
            try
            {
                while (await input.ReadLineAsync() is { } line)
                {
                    using var document = JsonDocument.Parse(line);
                    var message = document.RootElement.Clone();
                    if (!dispatcher.TryEnqueue(() =>
                    {
                        try { Receive(message); }
                        catch (Exception error)
                        {
                            Send(new { type = "ui_error", detail = error.Message });
                            Shutdown();
                        }
                    })) return;
                }
            }
            catch (IOException) { }
            catch (JsonException) { }
            finally { dispatcher.TryEnqueue(Shutdown); }
        });
    }

    internal void Send(object value)
    {
        try { output.WriteLine(JsonSerializer.Serialize(value)); }
        catch (IOException) { Shutdown(); }
    }

    private void Receive(JsonElement message)
    {
        switch (message.String("type"))
        {
            case "initialize":
                fields = message.Property("fields").EnumerateArray().Select(Field.From).ToArray();
                break;
            case "state":
                state = message.Property("state");
                status = message.String("status");
                sampling = message.String("sampling");
                if (full?.Shown == true) full.Apply(state, status, sampling);
                if (quick?.Shown == true) quick.Apply(state, status);
                break;
            case "show":
                lastPosition = new(message.Integer("x"), message.Integer("y"));
                if (message.String("surface") == "quick")
                {
                    quick ??= new QuickWindow(this);
                    if (quick.Shown && message.Boolean("toggle")) quick.Hide();
                    else
                    {
                        quick.Apply(state, status);
                        quick.Show(lastPosition);
                    }
                }
                else OpenFull();
                break;
            case "hide":
                quick?.Hide();
                if (message.String("surface") != "quick") full?.Hide();
                break;
            case "codex_result":
                full?.CodexResult(message.String("detail"), message.Boolean("error"));
                break;
            case "shutdown":
                Shutdown();
                break;
        }
    }

    internal void OpenFull(bool import = false)
    {
        quick?.Hide();
        full ??= new MainWindow(this, fields);
        full.Apply(state, status, sampling);
        full.Show(lastPosition);
        if (import)
        {
            full.Navigate(3);
            Send(new { type = "import" });
        }
    }

    private void Shutdown()
    {
        if (exiting) return;
        exiting = true;
        quick?.Close();
        full?.Close();
        Exit();
    }
}

internal static class Values
{
    internal static JsonElement Property(this JsonElement value, string name) =>
        value.ValueKind == JsonValueKind.Object && value.TryGetProperty(name, out var item) ? item : default;
    internal static string String(this JsonElement value, string name, string fallback = "") =>
        value.Property(name) is { ValueKind: JsonValueKind.String } text ? text.GetString() ?? fallback : fallback;
    internal static bool Boolean(this JsonElement value, string name, bool fallback = false) =>
        value.Property(name).ValueKind switch { JsonValueKind.True => true, JsonValueKind.False => false, _ => fallback };
    internal static int Integer(this JsonElement value, string name, int fallback = 0) =>
        value.Property(name) is { ValueKind: JsonValueKind.Number } number && number.TryGetInt32(out var result) ? result : fallback;
    internal static double Number(this JsonElement value, string name, double fallback = 0) =>
        value.Property(name) is { ValueKind: JsonValueKind.Number } number && number.TryGetDouble(out var result) ? result : fallback;
    internal static IEnumerable<JsonElement> Items(this JsonElement value, string name) =>
        value.Property(name) is { ValueKind: JsonValueKind.Array } list ? list.EnumerateArray().ToArray() : [];
}

internal sealed record Field(string Key, string Title, string Detail, string Icon, int Kind, int Page, double Min, double Max)
{
    internal static Field From(JsonElement value) => new(value.String("key"), value.String("title"), value.String("detail"),
        value.String("icon"), value.Integer("kind"), value.Integer("page"), value.Number("min"), value.Number("max"));
}
