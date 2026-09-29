using System.Text.Json;

namespace DwhOffWork;

public sealed record AppSettings(
    int StartMinutes = 9 * 60,
    int EndMinutes = 18 * 60,
    decimal DailySalary = 500m)
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        WriteIndented = true,
    };

    private static string SettingsPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "OffWork",
        "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (!File.Exists(SettingsPath))
            {
                return new AppSettings();
            }

            var json = File.ReadAllText(SettingsPath);
            var settings = JsonSerializer.Deserialize<AppSettings>(json, SerializerOptions);
            return settings is not null && settings.IsValid() ? settings : new AppSettings();
        }
        catch (JsonException)
        {
            return new AppSettings();
        }
        catch (IOException)
        {
            return new AppSettings();
        }
        catch (UnauthorizedAccessException)
        {
            return new AppSettings();
        }
    }

    public void Save()
    {
        var directory = Path.GetDirectoryName(SettingsPath)!;
        Directory.CreateDirectory(directory);
        File.WriteAllText(SettingsPath, JsonSerializer.Serialize(this, SerializerOptions));
    }

    public TimeOnly StartTime => TimeOnly.FromTimeSpan(TimeSpan.FromMinutes(StartMinutes));

    public TimeOnly EndTime => TimeOnly.FromTimeSpan(TimeSpan.FromMinutes(EndMinutes));

    private bool IsValid() =>
        StartMinutes is >= 0 and < 24 * 60 &&
        EndMinutes is >= 0 and < 24 * 60 &&
        StartMinutes != EndMinutes &&
        DailySalary >= 0;
}
