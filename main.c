#include <gtk/gtk.h>
#include <libserialport.h>
#include <time.h>


const char *serial_baud_rate_items[] = { "9600", "115200", NULL };
const char *serial_data_bits_items[] = { "5", "6", "7", "8", NULL };
const char *serial_parity_items[] = { "None", "Even", "Odd", NULL };
const char *serial_stop_bits_items[] = { "1", "1.5", "2", NULL };

const char *serial_receive_data_format_items[] = { "ASCII", "HEX", "ASCII & HEX", NULL};

// 下拉控件的全局变量

// 串口开启配置参数
GtkWidget *serial_ports_dropdown;
GtkWidget *serial_baud_rate_dropdown;
GtkWidget *serial_data_bits_dropdown;
GtkWidget *serial_parity_dropdown;
GtkWidget *serial_stop_bits_dropdown;

// 数据接收格式
GtkWidget *serial_receive_data_format_dropdown;

// 是否自动滚动数据接收
GtkWidget *auto_data_view_scroll_check_button;

// 是否自动添加时间戳
GtkWidget *auto_add_timestamp_check_button;


GtkWidget *button_open;
GtkWidget *text_view;
GtkTextBuffer *text_buffer;

struct sp_port *serial_port = NULL;
GThread *reader_thread = NULL;
gboolean port_open = FALSE;


void 
append_text(const char *text) {
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(text_buffer, &end);
  gtk_text_buffer_insert(text_buffer, &end, text, -1);

  // 根据用户是否勾选了 Auto Scroll 来判断是否需要滚动 text view
  gboolean auto_scroll_flag = gtk_check_button_get_active(GTK_CHECK_BUTTON (auto_data_view_scroll_check_button));

  if (auto_scroll_flag) {
    gtk_text_buffer_get_end_iter(text_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW (text_view), &end, 0.0, FALSE, 0, 0);
  }
  
}


gboolean 
append_text_idle(gpointer data) {
  char *text = (char *)data;
  append_text(text);
  g_free(text);
  return FALSE; // 只运行一次
}


gpointer 
read_serial_data(gpointer data) {
    char buf[256];
    while (port_open) {
        int len = sp_blocking_read(serial_port, buf, sizeof(buf) - 1, 100); // 缩短超时
        if (len > 0) {
            buf[len] = '\0';

            // 获取数据格式选项
            guint format_selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(serial_receive_data_format_dropdown));
            const char *format_str = serial_receive_data_format_items[format_selected];

            // 获取是否自动添加时间戳
            gboolean auto_add_timestamp_flag = gtk_check_button_get_active(GTK_CHECK_BUTTON (auto_add_timestamp_check_button));

            char output_buf[1024]; // 足够大的缓冲区
            output_buf[0] = '\0'; // 初始化为空字符串


            if (auto_add_timestamp_flag) {
                // 获取当前时间
                time_t rawtime;
                struct tm *timeinfo;
                time(&rawtime);
                timeinfo = localtime(&rawtime);

                // 格式化时间戳
                char timestamp[25]; // "yyyy-MM-dd HH:mm:ss\0"
                strftime(timestamp, sizeof(timestamp), "\n[%Y-%m-%d %H:%M:%S]", timeinfo);

                // 将时间戳添加到输出缓冲区
                strcat(output_buf, timestamp);
                strcat(output_buf, " "); // 添加一个空格分隔时间和数据
            }

            if (strcmp(format_str, "ASCII") == 0) {
                strcat(output_buf, buf);
            } 
            else if (strcmp(format_str, "HEX") == 0) {
                for (int i = 0; i < len; i++) {
                    char hex_str[4];
                    snprintf(hex_str, sizeof(hex_str), "%02X ", (unsigned char)buf[i]);
                    strcat(output_buf, hex_str);
                }
            } 
            else if (strcmp(format_str, "ASCII & HEX") == 0) {
                strcat(output_buf, "ASCII: ");
                strcat(output_buf, buf);
                strcat(output_buf, "\nHEX: ");
                for (int i = 0; i < len; i++) {
                    char hex_str[4];
                    snprintf(hex_str, sizeof(hex_str), "%02X ", (unsigned char)buf[i]);
                    strcat(output_buf, hex_str);
                }
            }

            g_idle_add(append_text_idle, g_strdup(output_buf));
        } else if (len < 0) {
            // 发生错误（如端口被关闭），立即退出循环
            break;
        }
    }
    return NULL;
}


void 
toggle_port(GtkButton *button, gpointer user_data) {
  if (!port_open) {

    // 用户选择的串口名字
    const char *port_name = gtk_string_list_get_string(
      GTK_STRING_LIST (
        gtk_drop_down_get_model(
          GTK_DROP_DOWN (serial_ports_dropdown)
        )
      ),
      gtk_drop_down_get_selected (
        GTK_DROP_DOWN(serial_ports_dropdown)
      )
    );

    // 用户选择的波特率、数据位、校验位、停止位
    guint baud_rate_selected = gtk_drop_down_get_selected(GTK_DROP_DOWN (serial_baud_rate_dropdown));
    guint data_bits_selected = gtk_drop_down_get_selected(GTK_DROP_DOWN (serial_data_bits_dropdown));
    guint parity_selected = gtk_drop_down_get_selected(GTK_DROP_DOWN (serial_parity_dropdown));
    guint stop_bits_selected = gtk_drop_down_get_selected(GTK_DROP_DOWN (serial_stop_bits_dropdown));


    int baud_rate = atoi(serial_baud_rate_items[baud_rate_selected]);
    int data_bits = atoi(serial_data_bits_items[data_bits_selected]);


    enum sp_parity parity;

    const char *parity_str = serial_parity_items[parity_selected];
    if (strcmp(parity_str, "Even") == 0) {
      parity = SP_PARITY_EVEN;
    } 
    else if (strcmp(parity_str, "Odd") == 0) {
      parity = SP_PARITY_ODD;
    } 
    else {
      parity = SP_PARITY_NONE;
    }

    int stop_bits;
    const char *stop_bits_str = serial_stop_bits_items[stop_bits_selected];
    if (strcmp(stop_bits_str, "1.5") == 0) {
        stop_bits = 3;
    } 
    else if (strcmp(stop_bits_str, "2") == 0) {
        stop_bits = 2;
    } 
    else {
        stop_bits = 1;
    }


    if (!port_name || sp_get_port_by_name(port_name, &serial_port) != SP_OK) {
      append_text("Error: Can't find port!\n");
      return;
    }

    if (sp_open(serial_port, SP_MODE_READ) != SP_OK) {
      char err_msg[64];
      snprintf(err_msg, sizeof(err_msg), "Open error: %s\n", sp_last_error_message());
      append_text(err_msg);
      sp_free_port(serial_port);
      serial_port = NULL;
      return;
    }

    sp_set_baudrate(serial_port, baud_rate);
    sp_set_bits(serial_port, data_bits);
    sp_set_parity(serial_port, parity);
    sp_set_stopbits(serial_port, stop_bits);

    port_open = TRUE;
    reader_thread = g_thread_new("reader", read_serial_data, NULL);
    gtk_button_set_label(button, "Close Port");
    append_text("Port opened successfully.\n");

  } 
  else {
    // 先标记关闭
    port_open = FALSE;
    // 强制中断读取操作
    sp_close(serial_port);
    g_thread_join(reader_thread);
    reader_thread = NULL;
    sp_free_port(serial_port);
    serial_port = NULL;
    gtk_button_set_label(button, "Open Port");
    append_text("Port closed.\n");
  }
}


/**
 * 找到所有可用的串口
 */
void 
refresh_ports() {

  struct sp_port **ports;
  enum sp_return result = sp_list_ports(&ports);

  if (result != SP_OK) {
    append_text("Error: Failed to list ports!\n");
    return;
  }

  GtkStringList *ports_str_list = gtk_string_list_new(NULL);
  int port_count = 0;
  
  for (int i = 0; ports[i] != NULL; i++) {
    const char *port_name = sp_get_port_name(ports[i]);
    gtk_string_list_append(ports_str_list, port_name);
    port_count++;
  }

  if (port_count == 0) {
    append_text("No serial ports found.\n");
  }
  else {
    char msg[64];
    snprintf(msg, sizeof(msg), "Found %d serial ports.\n", port_count);
    append_text(msg);
  }

  gtk_drop_down_set_model(GTK_DROP_DOWN (serial_ports_dropdown), G_LIST_MODEL (ports_str_list));
  sp_free_port_list(ports);
}


void
clear_content(GtkButton *button, gpointer user_data) {
  gtk_text_buffer_set_text(text_buffer, "", -1);
}



void 
save_file_callback(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data) {
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);
    
    if (file != NULL) {
        char *file_path = g_file_get_path(file);

        GtkTextIter start, end;
        gtk_text_buffer_get_start_iter(text_buffer, &start);
        gtk_text_buffer_get_end_iter(text_buffer, &end);
        char *text = gtk_text_buffer_get_text(text_buffer, &start, &end, FALSE);

        FILE *fp = fopen(file_path, "w");
        if (fp) {
            fprintf(fp, "%s", text);
            fclose(fp);
        } 
        else {
            // Show error dialog using GtkAlertDialog
            GtkWidget *parent = gtk_widget_get_ancestor(GTK_WIDGET(dialog), GTK_TYPE_WINDOW);
            GtkAlertDialog *alert = gtk_alert_dialog_new("Error saving file");
            gtk_alert_dialog_set_detail(alert, error->message);
            gtk_alert_dialog_set_buttons(alert, (const char *[]){"OK", NULL});
            gtk_alert_dialog_choose(alert, GTK_WINDOW(parent), NULL, NULL, NULL);
            g_object_unref(alert);
            g_error_free(error);
        }
        g_free(file_path);
        g_free(text);
        g_object_unref(file);
    }
}



void 
save_content(GtkButton *button, gpointer user_data) {
    
    // 获取顶层窗口
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_ancestor(GTK_WIDGET (button), GTK_TYPE_WINDOW));

    if (!parent) {
        g_printerr("Error: Could not find parent window.\n");
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save File");
    gtk_file_dialog_set_initial_folder(dialog, g_file_new_for_path(g_get_user_config_dir()));

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char filename[256];
    strftime(filename, sizeof(filename), "SerialData-%Y-%m-%d_%H-%M-%S.txt", timeinfo);
    gtk_file_dialog_set_initial_name(dialog, filename);

    gtk_file_dialog_save(dialog, parent, NULL, (GAsyncReadyCallback) save_file_callback, user_data);
    g_object_unref(dialog);

}


static void 
activate(GtkApplication *app, gpointer user_data) {
  int default_width = 800;
  int default_height = 600;

  // 主窗口
  GtkWidget *main_window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW (main_window), "Simple Serial Monitor");
  gtk_window_set_default_size(GTK_WINDOW (main_window), default_width, default_height);

  // 主 box
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_window_set_child(GTK_WINDOW (main_window), main_box);


  // 左侧-box-用来放置选项控件
  GtkWidget *options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_append(GTK_BOX (main_box), options_box);


  // 左侧-grid-用于设置串口的参数
  GtkWidget *serial_options_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID (serial_options_grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID (serial_options_grid), 5);
  gtk_box_append(GTK_BOX (options_box), serial_options_grid);

  // 左侧-grid-用于设置接收数据的参数
  GtkWidget *receive_data_options_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID (receive_data_options_grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID (receive_data_options_grid), 5);
  gtk_box_append(GTK_BOX (options_box), receive_data_options_grid);

  // 左侧-grid-用于设置发送数据的参数
  GtkWidget *send_data_options_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID (send_data_options_grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID (send_data_options_grid), 5);
  gtk_box_append(GTK_BOX (options_box), send_data_options_grid);



  // 右侧-box-用来放置串口数据收发的文本内容
  GtkWidget *serial_data_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_append(GTK_BOX (main_box), serial_data_box);


  // COM 串口选择下拉列表
  GtkWidget *choose_port_label = gtk_label_new("Choose Port");
  serial_ports_dropdown = gtk_drop_down_new(NULL, NULL);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_ports_dropdown);


  // 波特率选择下拉列表
  GtkWidget *baud_rate_label = gtk_label_new("Baud Rate");
  GtkStringList *serial_baud_rate_list = gtk_string_list_new(serial_baud_rate_items);
  serial_baud_rate_dropdown = gtk_drop_down_new(G_LIST_MODEL (serial_baud_rate_list), NULL);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_baud_rate_dropdown);


  // 数据位选择下拉列表
  GtkWidget *date_bits_label = gtk_label_new("Data Bits");
  serial_data_bits_dropdown = gtk_drop_down_new_from_strings(serial_data_bits_items);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_data_bits_dropdown);

  // 校验位选择下拉列表
  GtkWidget *parity_label = gtk_label_new("Parity");
  serial_parity_dropdown = gtk_drop_down_new_from_strings(serial_parity_items);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_parity_dropdown);

  //停止位选择下拉列表
  GtkWidget *stop_bits_label = gtk_label_new("Stop Bits");
  serial_stop_bits_dropdown = gtk_drop_down_new_from_strings(serial_stop_bits_items);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_stop_bits_dropdown);


  // 接收数据格式下拉列表
  GtkWidget *data_format_label = gtk_label_new("Data Format");
  serial_receive_data_format_dropdown = gtk_drop_down_new_from_strings(serial_receive_data_format_items);
  // gtk_box_append(GTK_BOX (serial_options_box), serial_receive_data_format_dropdown);


  // 是否自动滚动数据接收屏幕
  // auto_data_view_scroll_check_button = gtk_check_button_new_with_label("Auto Scroll");
  // gtk_box_append(GTK_BOX (serial_options_box), auto_data_view_scroll_check_button);
  GtkWidget *auto_scroll_label = gtk_label_new("Auto Scroll");
  auto_data_view_scroll_check_button = gtk_check_button_new();


  // 是否添加时间戳
  GtkWidget *auto_add_timestamp_label = gtk_label_new("Auto TimeStamp");
  auto_add_timestamp_check_button = gtk_check_button_new();


  // 开关串口按键
  button_open = gtk_button_new_with_label("Open Port");
  g_signal_connect(button_open, "clicked", G_CALLBACK (toggle_port), NULL);
  // gtk_box_append(GTK_BOX (serial_options_box), button_open);

  // 清屏按键
  GtkWidget *clear_content_button = gtk_button_new_with_label("Clear");
  g_signal_connect(clear_content_button, "clicked", G_CALLBACK (clear_content), NULL);


  // 保存按键
  GtkWidget *save_content_button = gtk_button_new_with_label("Save");
  g_signal_connect(save_content_button, "clicked", G_CALLBACK (save_content), NULL);



  // 串口参数 grid 设置
  gtk_grid_attach(GTK_GRID (serial_options_grid), gtk_label_new("Serial Config"), 0, 0, 2, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), choose_port_label,         0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID (serial_options_grid), serial_ports_dropdown,     1, 1, 1, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), baud_rate_label,           0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID (serial_options_grid), serial_baud_rate_dropdown, 1, 2, 1, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), date_bits_label,           0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID (serial_options_grid), serial_data_bits_dropdown, 1, 3, 1, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), parity_label,              0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID (serial_options_grid), serial_parity_dropdown,    1, 4, 1, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), stop_bits_label,           0, 5, 1, 1);
  gtk_grid_attach(GTK_GRID (serial_options_grid), serial_stop_bits_dropdown, 1, 5, 1, 1);

  gtk_grid_attach(GTK_GRID (serial_options_grid), button_open, 0, 6, 2, 1);


  // 接收数据参数 grid 设置
  gtk_grid_attach(GTK_GRID (receive_data_options_grid), gtk_label_new("Receive Config"), 0, 0, 2, 1);

  gtk_grid_attach(GTK_GRID (receive_data_options_grid), data_format_label,                   0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID (receive_data_options_grid), serial_receive_data_format_dropdown, 1, 1, 1, 1);

  gtk_grid_attach(GTK_GRID (receive_data_options_grid), auto_scroll_label,                  0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID (receive_data_options_grid), auto_data_view_scroll_check_button, 1, 2, 1, 1);

  gtk_grid_attach(GTK_GRID (receive_data_options_grid), auto_add_timestamp_label,        0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID (receive_data_options_grid), auto_add_timestamp_check_button, 1, 3, 1, 1);

  

  gtk_grid_attach(GTK_GRID (receive_data_options_grid), clear_content_button, 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID (receive_data_options_grid), save_content_button,  1, 4, 1, 1);
  


  // 发送数据参数 grid 设置


  // 右侧的串口数据发送/接口窗口
  GtkWidget *scrolled = gtk_scrolled_window_new();


  // 初始化 text view 和 text buffer
  text_view = gtk_text_view_new();
  text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

  // 不可编辑 text view
  gtk_text_view_set_editable(GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD_CHAR);

  // 设置 text view 作为 scrolled 的子节点
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW (scrolled), text_view);
  

  // 设置 scrolled 控件，水平/垂直都自动填满 box
  gtk_widget_set_hexpand(scrolled, TRUE);
  gtk_widget_set_vexpand(scrolled, TRUE);

  gtk_box_append(GTK_BOX (serial_data_box), scrolled);


  // 填充 COM 串口选择列表
  refresh_ports();

  gtk_window_present(GTK_WINDOW (main_window));
}


int
main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("site.djhx", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK (activate), NULL);
  int status = g_application_run(G_APPLICATION (app), argc, argv);
  g_object_unref(app);
  return status;
}