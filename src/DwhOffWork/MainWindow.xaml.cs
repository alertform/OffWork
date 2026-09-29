using System.Globalization;
using DwhOffWork.Core;
using Microsoft.UI;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using WinRT.Interop;
using Windows.Graphics;

namespace DwhOffWork;

public sealed partial class MainWindow : Window
{
    private const int WindowWidth = 360;
    private const int CollapsedHeight = 390;
    private const int ExpandedHeight = 720;

    private static readonly CultureInfo ChineseCulture = CultureInfo.GetCultureInfo("zh-CN");

    private readonly DispatcherTimer _timer;
    private readonly AppWindow _appWindow;
    private AppSettings _settings;

    public MainWindow()
    {
        InitializeComponent();

        Title = "OffWork";
        SystemBackdrop = new MicaBackdrop { Kind = MicaKind.BaseAlt };

        var windowHandle = WindowNative.GetWindowHandle(this);
        var windowId = Win32Interop.GetWindowIdFromWindow(windowHandle);
        _appWindow = AppWindow.GetFromWindowId(windowId);
        ConfigureWindow();
        Activated += (_, _) => ApplyPinState();

        _settings = AppSettings.Load();
        PopulateSettingsControls();

        _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _timer.Tick += (_, _) => RefreshDisplay(DateTime.Now);
        _timer.Start();

        Closed += (_, _) => _timer.Stop();
        RefreshDisplay(DateTime.Now);
    }

    private void ConfigureWindow()
    {
        _appWindow.IsShownInSwitchers = false;
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(TitleBarDragRegion);

        _appWindow.TitleBar.ButtonBackgroundColor = Colors.Transparent;
        _appWindow.TitleBar.ButtonInactiveBackgroundColor = Colors.Transparent;
        UpdateTitleBarInsets();
        _appWindow.TitleBar.LayoutMetricsChanged += (_, _) => UpdateTitleBarInsets();

        if (_appWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsAlwaysOnTop = true;
            presenter.IsResizable = false;
            presenter.IsMaximizable = false;
            presenter.IsMinimizable = false;
        }

        ResizeWindow(CollapsedHeight);
        MoveToTopRight();
    }

    private void MoveToTopRight()
    {
        var displayArea = DisplayArea.GetFromWindowId(_appWindow.Id, DisplayAreaFallback.Primary);
        if (displayArea is null)
        {
            return;
        }

        const int margin = 20;
        var workArea = displayArea.WorkArea;
        _appWindow.Move(new PointInt32(
            workArea.X + workArea.Width - WindowWidth - margin,
            workArea.Y + margin));
    }

    private void PopulateSettingsControls()
    {
        StartTimePicker.Time = TimeSpan.FromMinutes(_settings.StartMinutes);
        EndTimePicker.Time = TimeSpan.FromMinutes(_settings.EndMinutes);
        DailySalaryBox.Value = (double)_settings.DailySalary;
    }

    private void RefreshDisplay(DateTime now)
    {
        var snapshot = WorkdayCalculator.Calculate(
            now,
            _settings.StartTime,
            _settings.EndTime,
            _settings.DailySalary);

        CurrentTimeText.Text = now.ToString("M月d日  HH:mm:ss", ChineseCulture);
        WorkProgressBar.Value = snapshot.Progress * 100;
        ProgressText.Text = $"今日进度 {snapshot.Progress:P0}";
        EarnedText.Text = snapshot.Earned.ToString("C2", ChineseCulture);
        DailySalaryText.Text = $"日薪 {snapshot.DailySalary.ToString("C2", ChineseCulture)}";

        switch (snapshot.Phase)
        {
            case WorkdayPhase.BeforeWork:
                StatusLabel.Text = "距离上班";
                CountdownText.Text = FormatDuration(snapshot.Remaining);
                break;
            case WorkdayPhase.Working:
                StatusLabel.Text = "距离下班";
                CountdownText.Text = FormatDuration(snapshot.Remaining);
                break;
            case WorkdayPhase.AfterWork:
                StatusLabel.Text = "今天的工作完成啦";
                CountdownText.Text = "已下班 🎉";
                break;
        }
    }

    private static string FormatDuration(TimeSpan duration)
    {
        var totalHours = (int)duration.TotalHours;
        return $"{totalHours:00}:{duration.Minutes:00}:{duration.Seconds:00}";
    }

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {
        var startMinutes = (int)StartTimePicker.Time.TotalMinutes;
        var endMinutes = (int)EndTimePicker.Time.TotalMinutes;
        var salary = double.IsNaN(DailySalaryBox.Value)
            ? -1m
            : (decimal)DailySalaryBox.Value;

        if (startMinutes == endMinutes || salary < 0)
        {
            ValidationInfoBar.IsOpen = true;
            return;
        }

        ValidationInfoBar.IsOpen = false;
        _settings = new AppSettings(startMinutes, endMinutes, salary);

        try
        {
            _settings.Save();
        }
        catch (IOException)
        {
            ShowSaveError();
            return;
        }
        catch (UnauthorizedAccessException)
        {
            ShowSaveError();
            return;
        }

        RefreshDisplay(DateTime.Now);
        SettingsExpander.IsExpanded = false;
    }

    private void ShowSaveError()
    {
        ValidationInfoBar.Message = "保存失败，请检查本机用户目录的写入权限。";
        ValidationInfoBar.IsOpen = true;
    }

    private void PinButton_Click(object sender, RoutedEventArgs e)
    {
        ApplyPinState();
    }

    private void RootGrid_Loaded(object sender, RoutedEventArgs e) =>
        ApplyPinState();

    private void ApplyPinState()
    {
        if (_appWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsAlwaysOnTop = PinButton.IsChecked == true;
        }
    }

    private void UpdateTitleBarInsets()
    {
        TitleBarRightInset.Width = _appWindow.TitleBar.RightInset;
    }

    private void SettingsExpander_Expanding(
        Microsoft.UI.Xaml.Controls.Expander sender,
        Microsoft.UI.Xaml.Controls.ExpanderExpandingEventArgs e) =>
        ResizeWindow(ExpandedHeight);

    private void SettingsExpander_Collapsed(
        Microsoft.UI.Xaml.Controls.Expander sender,
        Microsoft.UI.Xaml.Controls.ExpanderCollapsedEventArgs e) =>
        ResizeWindow(CollapsedHeight);

    private void ResizeWindow(int height) =>
        _appWindow.Resize(new SizeInt32(WindowWidth, height));
}
