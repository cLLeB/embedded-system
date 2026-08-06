// Pulling in WinForms for the tray icon drags its whole namespace into every
// file through implicit usings, and two of its type names collide with ones
// this app uses far more often. Aliasing here beats repeating the fully
// qualified name at every use site, and beats dropping the tray icon.

global using LinkState = SmartSocket.App.Transport.LinkState;

// System.Drawing.Brush (WinForms) vs System.Windows.Media.Brush (WPF). Every
// brush in this app is a WPF one.
global using Brush = System.Windows.Media.Brush;
